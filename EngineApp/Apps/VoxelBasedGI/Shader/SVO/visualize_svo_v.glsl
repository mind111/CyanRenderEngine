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
	uint bSubdivide;
	uint bLeaf;
	uint childIndex;
	uint padding;
};

layout (std430, binding = 0) buffer SVONodeBuffer {
	SVONode nodes[];
};

uniform vec3 u_SVOCenter;
uniform vec3 u_SVOExtent;

void main()
{
	uint nodeIndex = gl_VertexID;
	SVONode node = nodes[nodeIndex];
	vec3 SVOOrigin = u_SVOCenter + vec3(-u_SVOExtent.x, -u_SVOExtent.y, u_SVOExtent.z);
	float nodeSize = (u_SVOExtent * 2.f / pow(2, node.coord.w)).x;
	vec3 nodeCenterPosition = SVOOrigin + vec3(node.coord.xy + .5f, -node.coord.z -.5f) * nodeSize;

	vsOut.position = nodeCenterPosition;
	vsOut.size = nodeSize;

	gl_Position = vec4(nodeCenterPosition, 1.f);
}
