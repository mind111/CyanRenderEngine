#version 450 core

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout (std430) buffer OpacityData
{
	int opacityMasks[];
} opacityBuffer;

layout(binding = 4, r32f) uniform image3D voxelGridOpacity;

void main()
{
	const uint superSamplingDim = 4;
	uint numSubSamples = superSamplingDim * superSamplingDim * superSamplingDim;
	uint start = (gl_WorkGroupID.x * gl_WorkGroupID.y * gl_WorkGroupID.z + gl_WorkGroupID.x * gl_WorkGroupID.y + gl_WorkGroupID.x) * numSubSamples;
	uint end = start + numSubSamples;
	float opacity = 0.f;
	for (uint i = start; i < end; ++i)
	{
		opacity += opacityBuffer.opacityMasks[i] * 1.f;
	}
	opacity /= numSubSamples;

	// write to image
	imageStore(voxelGridOpacity, ivec3(gl_WorkGroupID), vec4(opacity));
}
