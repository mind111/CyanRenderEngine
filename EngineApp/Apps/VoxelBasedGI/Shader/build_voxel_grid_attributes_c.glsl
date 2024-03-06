#version 450 core

layout (local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(binding = 0, r32f) uniform image3D u_voxelGridNormalXTex;
layout(binding = 1, r32f) uniform image3D u_voxelGridNormalYTex;
layout(binding = 2, r32f) uniform image3D u_voxelGridNormalZTex;
layout(binding = 3) writeonly uniform image3D u_voxelGridNormalTex; // rgba32f

layout(binding = 4, r32f) uniform image3D u_voxelGridRadianceR;
layout(binding = 5, r32f) uniform image3D u_voxelGridRadianceG;
layout(binding = 6, r32f) uniform image3D u_voxelGridRadianceB;
layout(binding = 7) writeonly uniform image3D u_voxelGridRadiance; // rgba32f

uniform sampler3D u_voxelGridFragmentCounter; // work around maximum of 8 image binding unit limitation

uniform ivec3 u_imageSize;

void main()
{
	ivec3 size = imageSize(u_voxelGridNormalTex);
	ivec3 dstCoord = ivec3(gl_GlobalInvocationID);

	vec3 normal = vec3(0.f);
	vec3 radiance = vec3(0.f);

	if (dstCoord.x < u_imageSize.x && dstCoord.y < u_imageSize.y && dstCoord.z < u_imageSize.z)
	{
		// calc average normal
		float normalX = imageLoad(u_voxelGridNormalXTex, dstCoord).r;
		float normalY = imageLoad(u_voxelGridNormalYTex, dstCoord).r;
		float normalZ = imageLoad(u_voxelGridNormalZTex, dstCoord).r;
		normal = vec3(normalX, normalY, normalZ);
		if (length(normal) > 0.f)
		{
			normal = normalize(normal);
		}
		imageStore(u_voxelGridNormalTex, dstCoord, vec4(normal, 0.f));

		float fragmentCounter = texelFetch(u_voxelGridFragmentCounter, dstCoord, 0).x;

		// calc average radiance
		float radianceR = imageLoad(u_voxelGridRadianceR, dstCoord).r;
		float radianceG = imageLoad(u_voxelGridRadianceG, dstCoord).r;
		float radianceB = imageLoad(u_voxelGridRadianceB, dstCoord).r;
		if (fragmentCounter > 0.f)
		{
			radiance = vec3(radianceR, radianceG, radianceB) / fragmentCounter;
		}
		imageStore(u_voxelGridRadiance, dstCoord, vec4(radiance, 0.f));
	}
}
