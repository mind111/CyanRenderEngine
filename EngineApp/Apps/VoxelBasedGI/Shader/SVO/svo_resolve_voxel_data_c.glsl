#version 450 core

layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

uniform sampler3D u_voxelAlbedoR;
uniform sampler3D u_voxelAlbedoG;
uniform sampler3D u_voxelAlbedoB;
uniform sampler3D u_voxelNormalX;
uniform sampler3D u_voxelNormalY;
uniform sampler3D u_voxelNormalZ;
uniform sampler3D u_voxelDirectLightingR;
uniform sampler3D u_voxelDirectLightingG;
uniform sampler3D u_voxelDirectLightingB;
uniform sampler3D u_perVoxelFragmentCounter;
uniform ivec3 u_voxelPoolSize;

uniform layout(rgba32f) image3D u_voxelAlbedoPool;
uniform layout(rgba32f) image3D u_voxelNormalPool;
uniform layout(rgba32f) image3D u_voxelDirectLightingPool;

ivec3 calcVoxelCoord(int index, ivec3 voxelPoolSize)
{
	int z = index / (voxelPoolSize.x * voxelPoolSize.y);
	int y = (index % (voxelPoolSize.x * voxelPoolSize.y)) / voxelPoolSize.x;
	int x = (index % (voxelPoolSize.x * voxelPoolSize.y)) % voxelPoolSize.x;
	return ivec3(x, y, z);
}

void main()
{
	int voxelIndex = int(gl_GlobalInvocationID.x);
	ivec3 voxelCoord = calcVoxelCoord(voxelIndex, u_voxelPoolSize);
	float albedoSumR = texelFetch(u_voxelAlbedoR, voxelCoord, 0).r;
	float albedoSumG = texelFetch(u_voxelAlbedoG, voxelCoord, 0).r;
	float albedoSumB = texelFetch(u_voxelAlbedoB, voxelCoord, 0).r;
	float normalSumX = texelFetch(u_voxelNormalX, voxelCoord, 0).r;
	float normalSumY = texelFetch(u_voxelNormalY, voxelCoord, 0).r;
	float normalSumZ = texelFetch(u_voxelNormalZ, voxelCoord, 0).r;
	float directLightingR = texelFetch(u_voxelDirectLightingR, voxelCoord, 0).r;
	float directLightingG = texelFetch(u_voxelDirectLightingG, voxelCoord, 0).r;
	float directLightingB = texelFetch(u_voxelDirectLightingB, voxelCoord, 0).r;
	float numVoxelFragments = texelFetch(u_perVoxelFragmentCounter, voxelCoord, 0).r;
	vec3 avgAlbedo = vec3(0.f), avgNormal = vec3(0.f), avgDirectLighting = vec3(0.f);
	if (numVoxelFragments > 0.f)
	{
		avgAlbedo = vec3(albedoSumR, albedoSumG, albedoSumB) / numVoxelFragments;
		avgNormal = normalize(vec3(normalSumX, normalSumY, normalSumZ));
		avgDirectLighting = vec3(directLightingR, directLightingG, directLightingB) / numVoxelFragments;
	}

	imageStore(u_voxelAlbedoPool, voxelCoord, vec4(avgAlbedo, 1.f));
	imageStore(u_voxelNormalPool, voxelCoord, vec4(avgNormal, 1.f));
	imageStore(u_voxelDirectLightingPool, voxelCoord, vec4(avgDirectLighting, 1.f));
}
