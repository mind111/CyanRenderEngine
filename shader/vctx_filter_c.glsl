#version 450 core

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

// mip n
uniform sampler3D srcAlbedo;
// mip n+1
layout(binding = 0) uniform writeonly image3D dstAlbedo;

uniform int srcMip;

const vec3 samples[8] = {
	vec3(-1.f, -1.f, -1.f),
	vec3( 1.f, -1.f, -1.f),
	vec3( 1.f, -1.f,  1.f),
	vec3(-1.f, -1.f,  1.f),
	vec3(-1.f,  1.f, -1.f),
	vec3( 1.f,  1.f, -1.f),
	vec3( 1.f,  1.f,  1.f),
	vec3(-1.f,  1.f,  1.f)
};

void main()
{
	// get texcoord for current voxel, and snap to voxel center
	vec3 voxelCoord = (vec3(gl_WorkGroupID) + .5f) / vec3(gl_NumWorkGroups);
	vec3 dVoxelCoord = vec3(1.f) / vec3(gl_WorkGroupID);
	ivec3 srcGridDim = textureSize(srcAlbedo, srcMip);

	vec4 avgAlbedo = vec4(0.f);
	int numFilledVoxels = 0;

	// sample 8 enclosing voxels, apply filtering and write to image
	for (int i = 0; i < 8; ++i)
	{
		ivec3 sampleCoord = ivec3((voxelCoord + samples[i] * .5f) * srcGridDim);
		vec4 albedo = texelFetch(srcAlbedo, sampleCoord, srcMip);
		avgAlbedo += albedo;
		numFilledVoxels += albedo.a > 0.f ? 1 : 0;
	}

	vec4 result = numFilledVoxels > 0 ? avgAlbedo / numFilledVoxels : vec4(0.f, 0.f, 0.f, 1.f);
	// write to dst mip 
	imageStore(dstAlbedo, ivec3(gl_WorkGroupID), result);
}