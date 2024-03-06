#version 450 core

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out VSOutput
{
	vec3 position;
	float size;
} vsOut;

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

	result.coord = ivec4(x, y, z, level);
	result.bSubdivide = ((1 << 29) & packedNode.data1) > 0;
	result.bLeaf = ((1 << 28) & packedNode.data1) > 0;
	result.childIndex = ((1 << 28) - 1) & packedNode.data1;

	return result;
}

uniform vec3 u_SVOCenter;
uniform vec3 u_SVOExtent;

void main()
{
	uint nodeIndex = gl_VertexID;
	SVONode node = unpack(packedNodes[nodeIndex]);
	vec3 SVOOrigin = u_SVOCenter + vec3(-u_SVOExtent.x, -u_SVOExtent.y, u_SVOExtent.z);
	float nodeSize = (u_SVOExtent * 2.f / pow(2, node.coord.w)).x;
	vec3 nodeCenterPosition = SVOOrigin + vec3(node.coord.xy + .5f, -node.coord.z -.5f) * nodeSize;

	vsOut.position = nodeCenterPosition;
	vsOut.size = nodeSize;

	gl_Position = vec4(nodeCenterPosition, 1.f);
}
