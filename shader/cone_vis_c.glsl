#version 450 core
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

layout(std430) buffer ConeTraceDebugData
{
    int numCubes;
    vec3 padding;
    vec4 coneDir[5];
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

uniform vec3 debugRayOrigin;

layout (binding = 10) uniform sampler3D sceneVoxelGridAlbedo;
layout (binding = 11) uniform sampler3D sceneVoxelGridNormal;
layout (binding = 12) uniform sampler3D sceneVoxelGridRadiance;

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

vec3 decodeHDR(vec3 color)
{
    return color / (vec3(1.f) - color);
}

struct ConeTraceRecord
{
    vec3 radiance;
    float occ;
};

ConeTraceRecord traceCone(vec3 p, vec3 rd, float halfAngle)
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

    while (alpha < 0.95f && isInsideBound(texCoord))
    {
        float coneDiameter = 2.f * traced * tanHalfAngle;
        float mip = log2(max(coneDiameter / sceneVoxelGrid.voxelSize, 1.f));
        float scale = pow(4.f, int(mip));

		vec4 albedo = textureLod(sceneVoxelGridAlbedo, texCoord, mip);
        vec4 radiance = textureLod(sceneVoxelGridRadiance, texCoord, mip);
        radiance.rgb = decodeHDR(radiance.rgb);

        float opacity = albedo.a;

        // this is necessary for correcting the "darkness" caused by auto generated mipmap included empty voxels in the average
        albedo *= scale;
        albedo /= albedo.a;
        radiance *= scale;
        radiance /= radiance.a;

		// emission-absorption model front to back blending
        // todo: integrated value is too dark
		occ = alpha * occ + (1.f - alpha) * opacity * 2.f;
        accAlbedo += (1.f - alpha) * albedo.rgb;
        accRadiance += (1.f - alpha) * radiance.rgb;
		alpha += (1.f - alpha) * opacity;

        // write cube data to buffer
        float sampleVoxelSize = sceneVoxelGrid.voxelSize * pow(2.f, floor(mip));
        numSteps++;

        // marching along the ray
        float stepSize = sampleVoxelSize;
		q += rd * stepSize;
        traced += stepSize;
        texCoord = getVoxelGridCoord(q);
    }
	float atten = 1.f / (1.f + traced);
    return ConeTraceRecord(accRadiance, occ);
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

ConeTraceRecord coneTraceHemisphere(vec3 p, vec3 n, int numTheta, int numPhi)
{
    // offset to next voxel in normal direction
    p += n * 2.0f * sceneVoxelGrid.voxelSize;

    vec3 radiance = vec3(0.f);
    float occ = 0.f;
    mat3 tbn = tbn(n);
    // compute half angle based on number of samples
    float halfAngle = .25f * pi / numTheta;
    float dTheta = .5f * pi / numTheta;
    float dPhi = 2.f * pi / numPhi;

    // the sample in normal direction
	vec3 dir = generateHemisphereSample(n, 0, 0);
	ConeTraceRecord record = traceCone(p, dir, halfAngle);
	occ += record.occ;
	radiance += record.radiance;

    for (int i = 0; i < numPhi; ++i)
    {
        for (int j = 0; j < numTheta; ++j)
        {
            float theta = (float(j) + .5f) * dTheta;
            float phi = (float(i) + .5f) * dPhi;
            dir = generateHemisphereSample(n, theta, phi);
			ConeTraceRecord record = traceCone(p, dir, halfAngle);
			occ += record.occ * max(dot(n, dir), 0.f);
			radiance += record.radiance;
        }
    }
    radiance /= (numTheta * numPhi + 1);
    occ /= (numTheta * numPhi + 1);
    return ConeTraceRecord(radiance, 1.f - min(occ, 1.f));
}

void main()
{
    // fill in indirect draw buffer
    indirectDrawBuffer.first = 0;
    indirectDrawBuffer.count = 0;
}