#version 450 core

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

layout (binding = 0) uniform atomic_uint u_newNodesCounter;

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
		atomicCounterIncrement(u_newNodesCounter);
	}
}
