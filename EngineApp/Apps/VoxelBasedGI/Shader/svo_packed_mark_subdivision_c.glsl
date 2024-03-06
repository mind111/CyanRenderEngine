#version 450

layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct VoxelFragment
{
	vec4 position; 
};

layout (std430, binding = 0) buffer VoxelFragmentBuffer {
	VoxelFragment voxelFragmentList[];
};

struct SVONode
{
	ivec4 coord;
	bool bSubdivide;
	bool bLeaf;
	uint childIndex;
};

struct PackedSVONode
{
	uint data0;
	uint data1;
};

layout (std430, binding = 1) buffer PackedSVONodePool {
	PackedSVONode packedNodes[];
};

SVONode unpack(in PackedSVONode packedNode)
{
	SVONode result;

	uint xMask = ((1 << 10) - 1) << 22;
	uint yMask = ((1 << 10) - 1) << 12;
	uint zMask = ((1 << 10) - 1) << 2;
	uint x = (packedNode.data0 & xMask) >> 22;
	uint y = (packedNode.data0 & yMask) >> 12;
	uint z = (packedNode.data0 & zMask) >> 2;
	uint levelA = packedNode.data0 & 0x3;
	uint levelB = (packedNode.data1 & ~((1 << 30) - 1)) >> 30;
	uint level = (levelA << 2) | levelB;

	result.coord = ivec4(x, y, z,level);
	result.bSubdivide = ((1 << 29) & packedNode.data1) > 0;
	result.bLeaf = ((1 << 28) & packedNode.data1) > 0;
	result.childIndex = ((1 << 28) - 1) & packedNode.data1;

	return result;
}

void markNodeForSubdivision(inout PackedSVONode node)
{
	node.data1 |= (1 << 29);
}

// uv should be in normalized 3d uv space
SVONode descendSVO(vec3 uv, int level, inout uint nodeIndex)
{
	SVONode node = unpack(packedNodes[0]);
	nodeIndex = 0;

	for (int i = 0; i < level; ++i)
	{
		// stop descending when reaching an empty node
		if (!node.bSubdivide)
		{
			break;
		}

		int childLevel = node.coord.w + 1;
		ivec3 childLevelSize = ivec3(pow(2, childLevel));

		ivec3 firstChildCoord = 2 * node.coord.xyz;
		ivec3 childNodeCoord = ivec3(floor(uv * childLevelSize)) - firstChildCoord;
		int childOffset = childNodeCoord.z * 4 + childNodeCoord.y * 2 + childNodeCoord.x;
		uint childIndex = node.childIndex + childOffset; 
		node = unpack(packedNodes[childIndex]);
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
	markNodeForSubdivision(packedNodes[outNodeIndex]);
}
