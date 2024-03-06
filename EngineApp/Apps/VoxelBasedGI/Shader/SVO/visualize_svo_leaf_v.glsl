#version 450 core

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out VSOutput
{
	float cull;
	vec3 position;
	float size;
	vec3 color;
} vsOut;

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

uniform vec3 u_SVOCenter;
uniform vec3 u_SVOExtent;
uniform sampler3D u_voxelAlbedoPool;

ivec3 calcVoxelCoord(int index, ivec3 voxelPoolSize)
{
	int z = index / (voxelPoolSize.x * voxelPoolSize.y);
	int y = (index % (voxelPoolSize.x * voxelPoolSize.y)) / voxelPoolSize.x;
	int x = (index % (voxelPoolSize.x * voxelPoolSize.y)) % voxelPoolSize.x;
	return ivec3(x, y, z);
}

void main()
{
	vsOut.cull = 1.f;
	vsOut.color = vec3(0.f);

	uint nodeIndex = gl_VertexID;
	SVONode node = nodes[nodeIndex];

	vec3 SVOOrigin = u_SVOCenter + vec3(-u_SVOExtent.x, -u_SVOExtent.y, u_SVOExtent.z);
	float nodeSize = (u_SVOExtent * 2.f / pow(2, node.coord.w)).x;
	vec3 nodeCenterPosition = SVOOrigin + vec3(node.coord.xy + .5f, -node.coord.z -.5f) * nodeSize;

	// if it's a leaf node
	if (node.bLeaf > 0 && node.bSubdivide > 0)
	{
		vsOut.cull = 0.f;
		ivec3 voxelPoolSize = textureSize(u_voxelAlbedoPool, 0);
		ivec3 voxelCoord = calcVoxelCoord(int(node.childIndex), voxelPoolSize);
		vsOut.color = node.coord.xyz / pow(2.f, node.coord.w);
		vec3 albedo = texelFetch(u_voxelAlbedoPool, voxelCoord, 0).rgb;
		vsOut.color = albedo;
	}

	vsOut.position = nodeCenterPosition;
	vsOut.size = nodeSize;
	gl_Position = vec4(nodeCenterPosition, 1.f);
}
