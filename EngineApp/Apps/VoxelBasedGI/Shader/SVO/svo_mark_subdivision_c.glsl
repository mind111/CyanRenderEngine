#version 450 core

layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct VoxelFragment
{
	vec4 position; 
	vec4 albedo;
	vec4 normal;
	vec4 directLighting;
};

layout (std430) buffer VoxelFragmentBuffer {
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

layout (std430) buffer SVONodeBuffer {
	SVONode nodes[];
};

// uv should be in normalized 3d uv space
SVONode descendSVO(vec3 uv, int level, inout uint nodeIndex)
{
	SVONode node = nodes[0];
	nodeIndex = 0;

	for (int i = 0; i < level; ++i)
	{
		// stop descending when reaching an empty node
		if (node.bSubdivide < 1)
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

uniform int u_currentLevel;
uniform vec3 u_SVOCenter;
uniform vec3 u_SVOExtent;
uniform vec3 u_SVOAABBMin;
uniform vec3 u_SVOAABBMax;

// for each voxel fragment, descend into the SVO to the current bottom most level and mark nodes as they need to be subdivided
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
	uint outNodeIndex = 0;
	SVONode node = descendSVO(uv, u_currentLevel, outNodeIndex);

	// mark the node for subdivision, having race condition here is fine
	nodes[outNodeIndex].bSubdivide = 1;
}
