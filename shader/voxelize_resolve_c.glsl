#version 450 core
#extension GL_NV_shader_atomic_float : require

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout (std430) buffer OpacityData
{
	int masks[];
} opacityBuffer;

layout(binding = 4, r32f) uniform image3D voxelGridOpacity;

uint getFlattendIndex(uvec3 texCoord, uvec3 dim)
{
	return texCoord.z * dim.x * dim.y + texCoord.y * dim.x + texCoord.x;
}

void main()
{
	const uint superSamplingScale = 4;
	uvec3 superSampledVoxelDim = uvec3(gl_NumWorkGroups) * superSamplingScale;
	uvec3 superSampledTexCoord = gl_WorkGroupID * superSamplingScale;

	float opacity = 0.f;
	for(uint i = 0; i < superSamplingScale; ++i)
	{
		for (uint j = 0; j < superSamplingScale; ++j)
		{
			for (uint k = 0; k < superSamplingScale; ++k)
			{
				opacity += float(opacityBuffer.masks[getFlattendIndex(superSampledTexCoord + uvec3(k, j, i), superSampledVoxelDim)]) * 1.f;
			}
		}
	}
	opacity /= pow(superSamplingScale, 3.f);

	// write to image
	if (opacity > 0.f)
		imageStore(voxelGridOpacity, ivec3(gl_WorkGroupID), vec4(opacity, 0.f, 0.f, 1.f));
}
