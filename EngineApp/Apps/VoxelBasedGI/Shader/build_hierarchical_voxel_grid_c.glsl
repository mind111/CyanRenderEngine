#version 450 core

layout (local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(binding = 0, r8) uniform image3D u_voxelGridOpacitySrcMip;
layout(binding = 1, r8) uniform image3D u_voxelGridOpacityDstMip;

uniform ivec3 u_imageSize;

void main()
{
	ivec3 size = imageSize(u_voxelGridOpacityDstMip);
	ivec3 dstCoord = ivec3(gl_GlobalInvocationID);
	float dstOpacity = 0.f;

	if (dstCoord.x < u_imageSize.x && dstCoord.y < u_imageSize.y && dstCoord.z < u_imageSize.z)
	{
		for (int i = 0; i < 8; ++i)
		{
			ivec3 localCoord;
			localCoord.z = i / 4;
			localCoord.y = (i - localCoord.z * 4) / 2;
			localCoord.x = (i - localCoord.z * 4) % 2;
			ivec3 srcCoord = ivec3(dstCoord) * 2 + localCoord;
			float srcOpacity = imageLoad(u_voxelGridOpacitySrcMip, srcCoord).r;
			if (srcOpacity > 0.f)
			{
				dstOpacity = srcOpacity;
			}
		}
		imageStore(u_voxelGridOpacityDstMip, dstCoord, vec4(dstOpacity));
	}
}
