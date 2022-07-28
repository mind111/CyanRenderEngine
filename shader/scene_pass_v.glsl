#version 450 core

#extension GL_ARB_shader_draw_parameters : enable 

out VSOutput
{
	vec3 viewSpacePosition;
	vec3 worldSpacePosition;
	vec3 worldSpaceNormal;
	vec3 viewSpaceTangent;
	vec2 texCoord0;
	vec2 texCoord1;
	vec3 color;
} vsOut;

#define VIEW_SSBO_BINDING 0
#define TRANSFORM_SSBO_BINDING 3
#define INSTANCE_DESC_BINDING 41
#define SUBMESH_BUFFER_BINDING 45
#define VERTEX_BUFFER_BINDING 42
#define INDEX_BUFFER_BINDING 43
#define DRAWCALL_BUFFER_BINDING 44

layout(std430, binding = VIEW_SSBO_BINDING) buffer ViewShaderStorageBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

layout(std430, binding = TRANSFORM_SSBO_BINDING) buffer TransformShaderStorageBuffer
{
    mat4 models[];
} transformSsbo;

struct Vertex
{
	vec4 pos;
	vec4 normal;
	vec4 tangent;
	vec4 texCoord;
};

layout(std430, binding = VERTEX_BUFFER_BINDING) buffer VertexBuffer
{
	Vertex vertices[];
} vertexBuffer;

layout(std430, binding = INDEX_BUFFER_BINDING) buffer IndexBuffer
{
	uint indices[];
} indexBuffer;

struct InstanceDesc
{
	uint submesh;
	uint material;
	uint transform;
	uint padding;
};

layout(std430, binding = INSTANCE_DESC_BINDING) buffer InstanceSSBO
{
	InstanceDesc descs[]; 
} instanceBuffer;

struct SubmeshDesc
{
	uint baseVertex;
	uint baseIndex;
	uint numVertices;
	uint numIndices;
};

layout(std430, binding = SUBMESH_BUFFER_BINDING) buffer SubmeshSSBO
{
	SubmeshDesc descs[];
} submeshBuffer;

layout(std430, binding = DRAWCALL_BUFFER_BINDING) buffer DrawCallSSBO
{
	uint drawCalls[];
} drawCallBuffer;

void main()
{
	uint instance = drawCallBuffer.drawCalls[gl_DrawIDARB] + gl_InstanceID;
	InstanceDesc desc = instanceBuffer.descs[instance];
	uint baseVertex = submeshBuffer.descs[desc.submesh].baseVertex;
	uint baseIndex = submeshBuffer.descs[desc.submesh].baseIndex;
	uint index = indexBuffer.indices[baseIndex + gl_VertexID];
	Vertex vertex = vertexBuffer.vertices[baseVertex + index];
	gl_Position = viewSsbo.projection * viewSsbo.view * transformSsbo.models[desc.transform] * vertex.pos;
	vsOut.color = vertex.normal.xyz * .5 + .5;
}
