#version 450
#extension GL_NV_shader_atomic_float : require 

layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct VoxelFragment
{
	vec4 position; 
	vec4 albedo;
	vec4 normal;
	vec4 directLighting;
};

layout (std430, binding = 0) buffer VoxelFragmentBuffer {
	VoxelFragment voxelFragmentList[];
};

struct SVONode
{
	ivec4 coord;
	uint bSubdivide;
	uint bLeaf;
	uint childIndex;
	uint padding;
};

layout (std430, binding = 1) buffer SVONodeBuffer {
	SVONode nodes[];
};

layout (r32f) uniform image3D u_voxelAlbedoR;
layout (r32f) uniform image3D u_voxelAlbedoG;
layout (r32f) uniform image3D u_voxelAlbedoB;
layout (r32f) uniform image3D u_voxelNormalX;
layout (r32f) uniform image3D u_voxelNormalY;
layout (r32f) uniform image3D u_voxelNormalZ;
layout (r32f) uniform image3D u_perVoxelFragmentCounter;

uniform vec3 u_SVOCenter;
uniform vec3 u_SVOExtent;
uniform vec3 u_SVOAABBMin;
uniform vec3 u_SVOAABBMax;
uniform int u_SVOMaxLevelCount;
uniform ivec3 u_voxelPoolSize;

// uv should be in normalized 3d uv space
SVONode descendSVO(vec3 uv, int level, inout uint nodeIndex)
{
	SVONode node = nodes[0];
	nodeIndex = 0;

	for (int i = 0; i < level; ++i)
	{
		// stop descending when reaching an empty node
		if (node.bSubdivide < 1 || node.bLeaf > 0)
		{
			break;
		}

		int childLevel = node.coord.w + 1;
		ivec3 childLevelSize = ivec3(pow(2, childLevel));

		ivec3 firstChildCoord = 2 * node.coord.xyz;
		ivec3 childNodeCoord = ivec3(floor(uv * childLevelSize)) - firstChildCoord;
		int childOffset = childNodeCoord.z * 4 + childNodeCoord.y * 2 + childNodeCoord.x;
		uint childIndex = node.childIndex + childOffset; 
		node = nodes[childIndex];
		nodeIndex = childIndex;
	}
	return node;
}

ivec3 calcVoxelCoord(int index, ivec3 voxelPoolSize)
{
	int z = index / (voxelPoolSize.x * voxelPoolSize.y);
	int y = (index % (voxelPoolSize.x * voxelPoolSize.y)) / voxelPoolSize.x;
	int x = (index % (voxelPoolSize.x * voxelPoolSize.y)) % voxelPoolSize.x;
	return ivec3(x, y, z);
}

void main()
{
	// fetch the fragment for this thread
	int voxelFragmentIndex = int(gl_GlobalInvocationID.x);
	VoxelFragment fragment = voxelFragmentList[voxelFragmentIndex];

	// calculate that normalized voxel fragment uv 
	vec3 SVOOrigin = u_SVOCenter + vec3(-u_SVOExtent.xy, u_SVOExtent.z);
	vec3 uv = (fragment.position.xyz - SVOOrigin) / (2.f * u_SVOExtent);
	uv.z *= -1.f;

	// descending into the SVO
	// todo: this descend procedure has bug, some voxel fragments fails to fall into a child node 
	uint outNodeIndex = 0;
	SVONode node = descendSVO(uv, u_SVOMaxLevelCount - 1, outNodeIndex);

	if (node.bLeaf > 0 && node.bSubdivide > 0)
	{
		ivec3 voxelCoord = calcVoxelCoord(int(node.childIndex), u_voxelPoolSize);

		imageAtomicAdd(u_voxelAlbedoR, voxelCoord, fragment.albedo.r);
		imageAtomicAdd(u_voxelAlbedoG, voxelCoord, fragment.albedo.g);
		imageAtomicAdd(u_voxelAlbedoB, voxelCoord, fragment.albedo.b);
		imageAtomicAdd(u_voxelNormalX, voxelCoord, fragment.normal.x);
		imageAtomicAdd(u_voxelNormalY, voxelCoord, fragment.normal.y);
		imageAtomicAdd(u_voxelNormalZ, voxelCoord, fragment.normal.z);
		imageAtomicAdd(u_perVoxelFragmentCounter, voxelCoord, 1.f);
	}
}
