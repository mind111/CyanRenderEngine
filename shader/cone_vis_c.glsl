﻿#version 450 core
/*
* This shader is used to generate debug visualization data for the following graphics pass to debug render
* the cone trace 
*/

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

#define pi 3.1415926

// output buffer
struct ConeCube
{
	vec3 center;
	float size;
    vec4 color;
};

layout(std430, binding = 0) buffer GlobalDrawData
{
    mat4  view;
    mat4  projection;
	mat4  sunLightView;
	mat4  sunShadowProjections[4];
    int   numDirLights;
    int   numPointLights;
    float m_ssao;
    float dummy;
} gDrawData;

layout(std430) buffer ConeTraceDebugData
{
    int numCubes;
    vec3 padding;
	ConeCube cubes[];
} debugConeBuffer;

layout(std430) buffer IndirectDrawArgs
{
    uint first; 
    uint count;
} indirectDrawBuffer;

//- voxel cone tracing
layout(std430, binding = 4) buffer VoxelGridData
{
    vec3 localOrigin;
    float voxelSize;
    int visMode;
    vec3 padding;
} sceneVoxelGrid;

uniform sampler2D sceneDepthTexture;
uniform sampler2D sceneNormalTexture;
layout (binding = 10) uniform sampler3D sceneVoxelGridAlbedo;
layout (binding = 11) uniform sampler3D sceneVoxelGridNormal;
layout (binding = 12) uniform sampler3D sceneVoxelGridRadiance;
layout (binding = 13) uniform sampler3D sceneVoxelGridOpacity;

uniform vec2 debugScreenPos;
uniform vec2 renderSize;
uniform mat4 cachedView;
uniform mat4 cachedProjection;

uniform struct VctxOptions
{
    float coneOffset;
    float useSuperSampledOpacity;
    float occlusionScale;
    float opacityScale;
    float indirectScale;
} opts;

mat3 tbn(vec3 n)
{
    vec3 up = abs(n.y) < 0.98f ? vec3(0.f, 1.f, 0.f) : vec3(0.f, 0.f, 1.f);
    vec3 right = cross(n, up);
    vec3 forward = cross(n, right);
    return mat3(
        right,
        forward,
        n
    );
}

vec3 decodeHDR(vec3 color)
{
    return color / (vec3(1.f) - color);
}

bool isInsideBound(vec3 texCoord)
{
	if ((texCoord.x < 0.f || texCoord.x > 1.f) || (texCoord.y < 0.f || texCoord.y > 1.f) ||
		(texCoord.z < 0.f || texCoord.z > 1.f))
	{
		return false;
	}
    return true;
}

vec3 getVoxelGridCoord(vec3 p)
{
    ivec3 voxelGridRes = textureSize(sceneVoxelGridAlbedo, 0);
    vec3 voxelGridDim = vec3(
		float(voxelGridRes.x) * sceneVoxelGrid.voxelSize,
		float(voxelGridRes.y) * sceneVoxelGrid.voxelSize,
		float(voxelGridRes.z) * sceneVoxelGrid.voxelSize
	);
    vec3 dd = p - sceneVoxelGrid.localOrigin;
    dd.z *= -1.f;
    return dd / voxelGridDim;
}

vec3 generateHemisphereSample(vec3 n, float theta, float phi)
{
	vec3 localDir = {
		sin(theta) * cos(phi),
		sin(theta) * sin(phi),
		cos(theta)
	};
	return tbn(n) * localDir;
}

/*
* Taken from https://thebookofshaders.com/10/
*/
float random (vec2 seed) {
    return fract(sin(dot(seed.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

float generateRandomRotation(vec2 seed)
{
    return random(seed) * 2.f * pi;
}

struct DebugTraceResult
{
    vec3 radiance;
    float occ;
    int numSteps;
};

DebugTraceResult traceCone(vec3 p, vec3 rd, float halfAngle, inout int totalNumSteps)
{
    float alpha = 0.f;
    float occ = 0.f;
    vec3 accRadiance = vec3(0.f);
    vec3 accAlbedo = vec3(0.f);

    vec3 q = p;
	vec3 texCoord = getVoxelGridCoord(q);
    float traced = 0.f;
    float tanHalfAngle = tan(halfAngle);
    int numSteps = 0;

    while (numSteps < 4 && alpha < 1.0f && isInsideBound(texCoord))
    {
        float coneDiameter = 2.f * traced * tanHalfAngle;
        float mip = log2(max(coneDiameter / sceneVoxelGrid.voxelSize, 1.f));
        float scale = pow(8.f, int(mip));

		vec4 albedo = textureLod(sceneVoxelGridAlbedo, texCoord, mip);
        vec4 radiance = textureLod(sceneVoxelGridRadiance, texCoord, mip);
        float opacitySS = textureLod(sceneVoxelGridOpacity, texCoord, mip).r;

        float opacity = albedo.a;

        // this is necessary for correcting the "darkness" caused by auto generated mipmap included empty voxels in the average
        albedo *= scale;
        albedo = albedo.a > 0.f ? (albedo / albedo.a) : vec4(0.f);

        radiance *= scale;
        radiance = radiance.a > 0.f ? (radiance / radiance.a) : vec4(0.f);
        radiance.rgb = decodeHDR(radiance.rgb);
        radiance /= mip;

        opacitySS *= scale;
        opacitySS = (albedo.a > 0.f) ? opacitySS / (albedo.a * scale) : 0.f;
        opacity = opacitySS * opts.opacityScale;

		// emission-absorption model front to back blending
		occ += (1.f - alpha) * opacity * opts.occlusionScale;
        accAlbedo += (1.f - alpha) * albedo.rgb;
        accRadiance += (1.f - alpha) * radiance.rgb;
		alpha += (1.f - alpha) * opacity;

        // write cube data to buffer
        float sampleVoxelSize = sceneVoxelGrid.voxelSize * pow(2.f, floor(mip));
        debugConeBuffer.cubes[totalNumSteps].center = q;
        debugConeBuffer.cubes[totalNumSteps].size = sampleVoxelSize;
        debugConeBuffer.cubes[totalNumSteps].color = vec4(radiance.rgb, 1.f);
        // debugConeBuffer.cubes[totalNumSteps].color = vec4(vec3(occ), 1.f);
        totalNumSteps++;
        numSteps++;

        // marching along the ray
        float stepSize = sampleVoxelSize;
		q += rd * stepSize;
        traced += stepSize;
        texCoord = getVoxelGridCoord(q);
    }
    return DebugTraceResult(accRadiance, occ, totalNumSteps);
}

DebugTraceResult sampleIrradianceAndOcclusion(vec3 p, vec3 n, int numTheta, int numPhi)
{
    int totalNumSteps = 0;

    // offset to next voxel in normal direction
    p += n * opts.coneOffset * sceneVoxelGrid.voxelSize;

    vec3 radiance = vec3(0.f);
    float occ = 0.f;
    mat3 tbn = tbn(n);
    // compute half angle based on number of samples
#if 0
    float halfAngle = .25f * pi / numTheta;
#else
    float halfAngle = pi / 6.f;
#endif
    float dTheta = .5f * pi / numTheta;
    float dPhi = 2.f * pi / numPhi;

    // the sample in normal direction
	vec3 dir = generateHemisphereSample(n, 0.f, 0.f);
	DebugTraceResult record = traceCone(p, dir, halfAngle, totalNumSteps);
	occ += record.occ;
	radiance += record.radiance;
    for (int i = 0; i < numPhi; ++i)
    {
        for (int j = 0; j < numTheta; ++j)
        {
            float theta = (float(j) + .5f) * dTheta;
            float phi = (float(i) + .5f) * dPhi + generateRandomRotation(debugScreenPos * renderSize);
            vec3 dir = generateHemisphereSample(n, theta, phi);
			DebugTraceResult record = traceCone(p, dir, halfAngle, totalNumSteps);
			occ += record.occ;
			radiance += record.radiance;
        }
    }
    radiance /= (numTheta * numPhi + 1);
    occ /= (numTheta * numPhi + 1);
    return DebugTraceResult(radiance, 1.f - min(occ, 1.f), totalNumSteps);
}

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) {
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    
    p = invView * p;
    return p.xyz;
}

void main()
{
    vec2 texCoord = debugScreenPos * .5f + .5f;
    float depth = texture(sceneDepthTexture, texCoord).r * 2.f - 1.f;
    vec3 worldPos = screenToWorld(vec3(texCoord * 2.f - 1.f, depth), inverse(cachedView), inverse(cachedProjection));
    vec3 n = normalize(texture(sceneNormalTexture, texCoord).rgb * 2.f - 1.f);

    DebugTraceResult result = sampleIrradianceAndOcclusion(worldPos, n, 0, 0);
    debugConeBuffer.numCubes = result.numSteps;

    // fill in indirect draw buffer
    indirectDrawBuffer.first = 0;
    indirectDrawBuffer.count = result.numSteps;
}