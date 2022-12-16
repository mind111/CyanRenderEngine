#version 450 core

#extension GL_ARB_shader_draw_parameters : enable 
#extension GL_ARB_gpu_shader_int64 : enable 

struct PBRMaterial
{
	uint64_t diffuseMap;
	uint64_t normalMap;
	uint64_t metallicRoughnessMap;
	uint64_t occlusionMap;
	vec4 kAlbedo;
	vec4 kMetallicRoughness;
	uvec4 flags;
};

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};
out VertexData
{
	vec3 viewSpacePosition;
	vec3 worldSpacePosition;
	vec3 worldSpaceNormal;
	vec3 worldSpaceTangent;
	flat float tangentSpaceHandedness;
	vec2 texCoord0;
	flat PBRMaterial material;
} vsOut;

#define VIEW_SSBO_BINDING 0
#define TRANSFORM_SSBO_BINDING 3
#define INSTANCE_DESC_BINDING 41
#define SUBMESH_BUFFER_BINDING 42
#define VERTEX_BUFFER_BINDING 43
#define INDEX_BUFFER_BINDING 44
#define DRAWCALL_BUFFER_BINDING 45
#define MATERIAL_BUFFER_BINDING 46

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
};

struct InstanceDesc
{
	uint submesh;
	uint material;
	uint transform;
	uint padding;
};

layout(std430, binding = INSTANCE_DESC_BINDING) buffer InstanceSSBO
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

layout(std430, binding = SUBMESH_BUFFER_BINDING) buffer SubmeshSSBO
{
	SubmeshDesc submeshDescs[];
};

layout(std430, binding = MATERIAL_BUFFER_BINDING) buffer MaterialSSBO
{
	PBRMaterial materials[];
};

layout(std430, binding = DRAWCALL_BUFFER_BINDING) buffer DrawCallSSBO
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

	gl_Position = transformSsbo.models[instance.transform] * vertex.pos;

	vsOut.worldSpacePosition = (transformSsbo.models[instance.transform] * vertex.pos).xyz;
	vsOut.worldSpaceNormal = normalize((inverse(transpose(transformSsbo.models[instance.transform])) * vertex.normal).xyz);
	vsOut.worldSpaceTangent = normalize((transformSsbo.models[instance.transform] * vec4(vertex.tangent.xyz, 0.f)).xyz);
	vsOut.tangentSpaceHandedness = vertex.tangent.w;
	vsOut.texCoord0 = vertex.texCoord.xy;
	vsOut.material = materials[instance.material];
}
