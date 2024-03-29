#version 450 core

#define pi 3.1415926

layout (location = 0) out vec3 vctxOcclusion;
layout (location = 1) out vec3 vctxIrradiance; 
layout (location = 2) out vec3 vctxReflection;

uniform vec2 renderSize;

uniform struct VctxOptions
{
    float coneOffset;
    float useSuperSampledOpacity;
    float occlusionScale;
    float opacityScale;
    float indirectScale;
} opts;

layout (binding = 10) uniform sampler3D sceneVoxelGridAlbedo;
layout (binding = 11) uniform sampler3D sceneVoxelGridNormal;
layout (binding = 12) uniform sampler3D sceneVoxelGridRadiance;
layout (binding = 13) uniform sampler3D sceneVoxelGridOpacity;
uniform sampler2D sceneDepthTexture;
uniform sampler2D sceneNormalTexture;

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

//- voxel cone tracing
layout(std430, binding = 4) buffer VoxelGridData
{
    vec3 localOrigin;
    float voxelSize;
    int visMode;
    vec3 padding;
} sceneVoxelGrid;

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

struct TraceResult
{
    vec3 radiance;
    float occ;
};

TraceResult traceCone(vec3 p, vec3 rd, float halfAngle)
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

    while (alpha < 1.f && isInsideBound(texCoord))
    {
        float coneDiameter = 2.f * traced * tanHalfAngle;
        float mip = log2(max(coneDiameter / sceneVoxelGrid.voxelSize, 1.f));
        float scale = pow(8.f, floor(mip));

		vec4 albedo = textureLod(sceneVoxelGridAlbedo, texCoord, mip);
        vec4 radiance = textureLod(sceneVoxelGridRadiance, texCoord, mip);
        float opacitySS = textureLod(sceneVoxelGridOpacity, texCoord, mip).r;

        float opacity = albedo.a * opts.opacityScale;

        // this is necessary for correcting the "darkness" caused by auto generated mipmap included empty voxels in the average
        albedo *= scale;
        albedo = albedo.a > 0.f ? (albedo / albedo.a) : vec4(0.f);

        radiance *= scale;
        radiance = radiance.a > 0.f ? (radiance / radiance.a) : vec4(0.f);
        radiance.rgb = decodeHDR(radiance.rgb);

        opacitySS *= scale;
        opacitySS = (albedo.a > 0.f) ? opacitySS / (albedo.a * scale) : 0.f;
        opacity = opacitySS * opts.opacityScale;

		// emission-absorption model front to back blending
#if 0
		occ = alpha * occ + (1.f - alpha) * opacity * opts.occlusionScale;
        accRadiance = alpha * accRadiance + (1.f - alpha) * radiance.rgb * opts.indirectScale;
		alpha += (1.f - alpha) * opacity;
#else
		occ += (1.f - alpha) * opacity * opts.occlusionScale;
        accRadiance += (1.f - alpha) * radiance.rgb * opts.indirectScale;
		alpha += (1.f - alpha) * opacity;
#endif

        // write cube data to buffer
        float sampleVoxelSize = sceneVoxelGrid.voxelSize * pow(2.f, floor(mip));
        numSteps++;

        // marching along the ray
        float stepSize = sampleVoxelSize;
		q += rd * stepSize;
        traced += stepSize;
        texCoord = getVoxelGridCoord(q);
    }
    return TraceResult(accRadiance, occ);
}

// todo: want to achieve fuzzy far field occlusion or sky occlusion, right now dynamic occlusion looks pretty much just like ssao
// todo: have a dedicated opacity texture ...? store super sampled opacity in each voxel of the finalized volume texture
// todo: add a control for cone aperture
TraceResult sampleIrradianceAndOcclusion(vec3 p, vec3 n, int numTheta, int numPhi)
{
    // offset to next voxel in normal direction
    p += n * opts.coneOffset * sceneVoxelGrid.voxelSize;
    float occlusion = 0.f;

    vec3 radiance = vec3(0.f);
    float occ = 0.f;
    float vis = 0.f;
    mat3 tbn = tbn(n);
    // compute half angle based on number of samples
    // float halfAngle = .25f * pi / numTheta;
    float halfAngle = pi / 6.f;
    float dTheta = .5f * pi / numTheta;
    float dPhi = 2.f * pi / numPhi;

    // the sample in normal direction
	vec3 dir = generateHemisphereSample(n, 0.f, 0.f);
	TraceResult record = traceCone(p, dir, halfAngle);
	occ += record.occ;
	radiance += record.radiance;

    for (int i = 0; i < numPhi; ++i)
    {
        for (int j = 0; j < numTheta; ++j)
        {
            float theta = (float(j) + .5f) * dTheta;
            float phi = (float(i) + .5f) * dPhi + generateRandomRotation(gl_FragCoord.xy);
            vec3 dir = generateHemisphereSample(n, theta, phi);
			TraceResult record = traceCone(p, dir, halfAngle);
			occ += record.occ * max(dot(dir, n), 0.f);
            // this alt occlusion looks much better!!
            occlusion += smoothstep(-0.8f, 1.f, sqrt(record.occ));
			radiance += record.radiance * max(dot(dir, n), 0.f);
        }
    }
    radiance /= (numTheta * numPhi + 1);
    occ /= (numTheta * numPhi + 1);
    occlusion /= (numTheta * numPhi + 1);
    return TraceResult(radiance, 1.f - occlusion);
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
    vec2 texCoord = gl_FragCoord.xy / renderSize;
    float depth = texture(sceneDepthTexture, texCoord).r * 2.f - 1.f;
    vec3 worldPos = screenToWorld(vec3(texCoord * 2.f - 1.f, depth), inverse(gDrawData.view), inverse(gDrawData.projection));
    vec3 n = normalize(texture(sceneNormalTexture, texCoord).rgb * 2.f - 1.f);

    TraceResult result = sampleIrradianceAndOcclusion(worldPos, n, 3, 6);

    vctxOcclusion = vec3(result.occ);
    vctxIrradiance = result.radiance;
}
