#version 450 core

#define PI 3.1415926

layout (local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
layout(binding = 0, r8) uniform image3D u_voxelGridFilteredOpacity;

uniform sampler3D u_voxelGridOpacity;
uniform ivec3 u_imageSize;

void main()
{
	ivec3 voxelCoord = ivec3(gl_GlobalInvocationID);
	if (voxelCoord.x < u_imageSize.x && voxelCoord.y < u_imageSize.y && voxelCoord.z < u_imageSize.z)
	{
		float opacity = texelFetch(u_voxelGridOpacity, voxelCoord, 0).r;
		imageStore(u_voxelGridFilteredOpacity, voxelCoord, vec4(opacity));
	}
}
