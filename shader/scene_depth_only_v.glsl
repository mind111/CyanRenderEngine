#version 450 core

#extension GL_ARB_shader_draw_parameters : enable 

/**
* scene shader storage buffers
*/

layout(std430) buffer ViewBuffer 
{
    mat4  view;
    mat4  projection;
};

layout(std430) buffer TransformBuffer 
{
    mat4 transforms[];
};

struct Vertex 
{
	vec4 pos;
	vec4 normal;
	vec4 tangent;
	vec4 texCoord;
};

layout(std430) buffer VertexBuffer 
{
	Vertex vertices[];
};

layout(std430) buffer IndexBuffer 
{
	uint indices[];
};

struct InstanceDesc 
{
	uint submesh;
	uint material;
	uint transform;
	uint padding;
};

layout(std430) buffer InstanceBuffer 
{
	InstanceDesc instanceDescs[];
};

struct SubmeshDesc 
{
	// uint type;
	uint baseVertex;
	uint numVertices;
	uint baseIndex;
	uint numIndices;
	// uint padding;
};

layout(std430) buffer SubmeshBuffer 
{
	SubmeshDesc submeshDescs[];
};

layout(std430) buffer InstanceLUTBuffer 
{
	uint instanceLUT[];
};

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

void main()
{
	uint instanceIndex = instanceLUT[gl_DrawIDARB] + gl_InstanceID;
	InstanceDesc instance = instanceDescs[instanceIndex];
	uint baseVertex = submeshDescs[instance.submesh].baseVertex;
	uint baseIndex = submeshDescs[instance.submesh].baseIndex;
	uint index = indices[baseIndex + gl_VertexID];
	Vertex vertex = vertices[baseVertex + index];
	gl_Position = projection * view * transforms[instance.transform] * vertex.pos;
}
