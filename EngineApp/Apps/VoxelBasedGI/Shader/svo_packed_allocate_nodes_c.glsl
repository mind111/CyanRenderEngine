#version 460 core

layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

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

layout (binding = 1) uniform atomic_uint u_allocatedNodesCounter;

uniform int u_SVONodePoolSize;
uniform int u_currentLevel;
uniform int u_SVOMaxLevelCount;

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

	result.coord = ivec4(x, y, z, level);
	result.bSubdivide = ((1 << 29) & packedNode.data1) > 0;
	result.bLeaf = ((1 << 28) & packedNode.data1) > 0;
	result.childIndex = ((1 << 28) - 1) & packedNode.data1;

	return result;
}

PackedSVONode pack(in SVONode node)
{
	PackedSVONode result;
	result.data0 = 0x0;
	result.data1 = 0x0;

	result.data0 = (node.coord.x << 22) | (node.coord.y << 12) | (node.coord.z << 2);
	result.data0 &= ~0x3;
	result.data0 |= ((0x3 << 2) & node.coord.w) >> 2;

	result.data1 &= ~(0x3 << 30);
	result.data1 |= ((0x3 & node.coord.w) << 30);

	result.data1 &= ~(0x1 << 29);
	if (node.bSubdivide)
	{
		result.data1 |= 0x1 << 29;
	}

	result.data1 &= ~(0x1 << 28);
	if (node.bLeaf)
	{
		result.data1 |= 0x1 << 28;
	}

	result.data1 &= ~((0x1 << 28) - 1);
	// clear 4 msb bits
	node.childIndex &= ~(0xF << 28);
	result.data1 |= node.childIndex;

	return result;
}

void setChildIndex(inout PackedSVONode packedNode, uint childIndex)
{
	packedNode.data1 &= ~((1 << 28) - 1);
	childIndex &= ~(0xF << 28);
	packedNode.data1 |= childIndex;
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

void main()
{
	int size = int(pow(2, u_currentLevel));
	ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
	if (coord.x >= size || coord.y >= size || coord.z >= size)
	{
		return;
	}

	// todo: debug this flip the y coord
	coord.y = size - int(gl_GlobalInvocationID.y + 1);
	vec3 uv = (coord + .5f) / size;

	// fetch the node that's assigned to this thread
	uint nodeIndex;
	SVONode node = descendSVO(uv, u_currentLevel, nodeIndex);

	// allocate child node tile if this node is marked for subdivision 
	if (node.bSubdivide)
	{
		uint firstChildIndex = atomicCounterAdd(u_allocatedNodesCounter, 8u);

		// setup child pointer 
		setChildIndex(packedNodes[nodeIndex], firstChildIndex);

		// todo: how to signal that we need to allocate a bigger node pool?
		if ((firstChildIndex + 8) < u_SVONodePoolSize)
		{
			ivec3 firstChildCoord = coord * 2;
			// initialize all child nodes
			for (int childOffset = 0; childOffset < 8; ++childOffset)
			{
				ivec3 tileCoord;
				tileCoord.z = childOffset / 4;
				tileCoord.y = (childOffset % 4) / 2;
				tileCoord.x = (childOffset % 4) % 2;

				SVONode childNode;
				childNode.coord = ivec4(firstChildCoord + tileCoord, u_currentLevel + 1);
				childNode.bSubdivide = false;
				childNode.bLeaf = (u_currentLevel == (u_SVOMaxLevelCount - 2));
				childNode.childIndex = 0;

				packedNodes[firstChildIndex + childOffset] = pack(childNode);
			}
		}
	}
}
