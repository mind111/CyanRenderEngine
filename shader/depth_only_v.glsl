#version 450 core

#extension GL_ARB_shader_draw_parameters : enable 

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

layout(std430) buffer TransformBuffer
{
    mat4 models[];
} transformSsbo;

struct Vertex {
	vec4 pos;
	vec4 normal;
	vec4 tangent;
	vec4 texCoord;
};

layout(std430) buffer VertexBuffer
{
	Vertex vertices[];
} vertexBuffer;

layout(std430) buffer IndexBuffer
{
	uint indices[];
};

layout(std430) buffer ViewBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

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
	uint baseVertex;
	uint baseIndex;
	uint numVertices;
	uint numIndices;
};

layout(std430) buffer SubmeshBuffer
{
	SubmeshDesc submeshDescs[];
};

layout(std430) buffer DrawCallBuffer
{
	uint drawCalls[];
};

void main() 
{
	uint instanceIndex = drawCalls[gl_DrawIDARB] + gl_InstanceID;
	InstanceDesc instance = instanceDescs[instanceIndex];
	uint baseVertex = submeshDescs[instance.submesh].baseVertex;
	uint baseIndex = submeshDescs[instance.submesh].baseIndex;
	uint index = indices[baseIndex + gl_VertexID];
	Vertex vertex = vertexBuffer.vertices[baseVertex + index];
	gl_Position = viewSsbo.projection * viewSsbo.view * transformSsbo.models[instance.transform] * vertex.pos;
}
