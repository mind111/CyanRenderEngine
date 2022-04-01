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

bool isInsideBound(vec3 texCoord)
{
	if ((texCoord.x < 0.f || texCoord.x > 1.f) || (texCoord.y < 0.f || texCoord.y > 1.f) ||
		(texCoord.z < 0.f || texCoord.z > 1.f))
	{
		return false;
	}
    return true;
}

vec3 encodeHDR(vec3 color)
{
	return color / (color + vec3(1.f));
}

vec3 decodeHDR(vec3 color)
{
    return color / (vec3(1.f) - color);
}

void debugCone60(vec3 p, vec3 rd)
{
    float alpha = 0.f;
    float occ = 0.f;
    vec3 accRadiance = vec3(0.f);
    vec3 accAlbedo = vec3(0.f);

    vec3 q = p;
	vec3 texCoord = getVoxelGridCoord(q);
    float traced = 0.f;
    float tan30 = tan(pi / 12.f);
    int numSteps = 0;

    while (alpha < 0.95f && isInsideBound(texCoord))
    {
        float coneDiameter = 2.f * traced * tan30;
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
		occ += (1.f - alpha) * opacity;
        accAlbedo += (1.f - alpha) * albedo.rgb;
        accRadiance += (1.f - alpha) * radiance.rgb;
		alpha += (1.f - alpha) * opacity;

        // write cube data to buffer
        float sampleVoxelSize = sceneVoxelGrid.voxelSize * pow(2.f, floor(mip));
        debugConeBuffer.cubes[numSteps].center = q;
        debugConeBuffer.cubes[numSteps].size = sampleVoxelSize;
        debugConeBuffer.cubes[numSteps].color = vec4(radiance.rgb, 1.f);
        numSteps++;

        // marching along the ray
        float stepSize = sampleVoxelSize;
		q += rd * stepSize;
        traced += stepSize;
        texCoord = getVoxelGridCoord(q);
    }
	debugConeBuffer.numCubes = numSteps;
}

struct ConeTraceRecord
{
    vec3 radiance;
    float occ;
};

ConeTraceRecord cone60(vec3 p, vec3 rd)
{
    float alpha = 0.f;
    float occ = 0.f;
    vec3 accRadiance = vec3(0.f);
    vec3 accAlbedo = vec3(0.f);

    vec3 q = p;
	vec3 texCoord = getVoxelGridCoord(q);
    float traced = 0.f;
    float tan30 = tan(pi / 12.f);
    int numSteps = 0;

    while (alpha < 0.95f && isInsideBound(texCoord))
    {
        float coneDiameter = 2.f * traced * tan30;
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
		occ += (1.f - alpha) * opacity;
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
    return ConeTraceRecord(accRadiance, occ);
}

void main()
{
    debugCone60(debugRayOrigin, vec3(0.f, 0.f, -1.f));
}