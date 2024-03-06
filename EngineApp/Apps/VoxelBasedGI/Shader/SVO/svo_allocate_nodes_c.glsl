#version 460 core

layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

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

layout (binding = 0) uniform atomic_uint u_allocatedNodesCounter;
layout (binding = 1) uniform atomic_uint u_allocatedVoxelsCounter;

uniform int u_SVONodePoolSize;
uniform int u_currentLevel;
uniform int u_SVONumLevels;

// uv should be in normalized 3d uv space
SVONode descendSVO(vec3 uv, int level, inout uint nodeIndex)
{
	SVONode node = nodes[0];
	nodeIndex = 0;

	for (int i = 0; i < level; ++i)
	{
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

void main()
{
	int size = int(pow(2, u_currentLevel));
	ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
	if (coord.x >= size || coord.y >= size || coord.z >= size)
	{
		return;
	}

	coord.y = size - int(gl_GlobalInvocationID.y + 1);
	vec3 uv = (coord + .5f) / size;

	// fetch the node that's assigned to this thread
	uint nodeIndex;
	SVONode node = descendSVO(uv, u_currentLevel, nodeIndex);

	// allocate child node tile if this node is marked for subdivision 
	if (node.bSubdivide > 0)
	{
		// the very last pass will mark non-empty leafs for subdivision, allocate
		// a voxel for each non-empty voxels
		if (u_currentLevel == (u_SVONumLevels - 1))
		{
			nodes[nodeIndex].childIndex = atomicCounterIncrement(u_allocatedVoxelsCounter);
		}
		else
		{
			uint firstChildIndex = atomicCounterAdd(u_allocatedNodesCounter, 8u);

			// setup child pointer 
			nodes[nodeIndex].childIndex = firstChildIndex;

			// todo: how to signal that we need to allocate a bigger node pool?
			if ((firstChildIndex + 7) < u_SVONodePoolSize)
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
					childNode.bSubdivide = 0;
					childNode.childIndex = 0;
					// mark leaf
					if (u_currentLevel == (u_SVONumLevels - 2))
					{
						childNode.bLeaf = 1;
					}
					else
					{
						childNode.bLeaf = 0;
					}
					childNode.padding = 0;

					nodes[firstChildIndex + childOffset] = childNode;
				}
			}
		}
	}
}
