#version 450 core

#extension GL_ARB_shader_draw_parameters : enable 
#extension GL_ARB_gpu_shader_int64 : enable 

/**
* scene shader storage buffers
*/

layout(std430) buffer ViewBuffer 
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
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
	uint baseVertex;
	uint baseIndex;
	uint numVertices;
	uint numIndices;
};

layout(std430) buffer SubmeshBuffer 
{
	SubmeshDesc submeshDescs[];
};

/**
	mirror's the material definition on application side
    struct GpuMaterial {
        u64 albedoMap;
        u64 normalMap;
        u64 metallicRoughnessMap;
        u64 occlusionMap;
        glm::vec4 albedo = glm::vec4(.9f, .9f, .9f, 1.f);
        f32 metallic = 0.f;
        f32 roughness = .5f;
        f32 emissive = 1.f;
        u32 flag = 0u;
    };
*/
struct MaterialDesc {
	uint64_t albedoMap;
	uint64_t normalMap;
	uint64_t metallicRoughnessMap;
    uint64_t occlusionMap;
    vec4 albedo;
    float metallic;
    float roughness;
    float emissive;
    uint flag;
};

layout(std430) buffer MaterialBuffer
{
	MaterialDesc materialDescs[];
};

layout(std430) buffer DrawCallBuffer 
{
	uint drawCalls[];
};

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};
out vec3 worldSpaceNormal;
out vec3 worldSpaceTangent;

void main() 
{
	uint instanceIndex = drawCalls[gl_DrawIDARB] + gl_InstanceID;
	InstanceDesc instance = instanceDescs[instanceIndex];
	uint baseVertex = submeshDescs[instance.submesh].baseVertex;
	uint baseIndex = submeshDescs[instance.submesh].baseIndex;
	uint index = indices[baseIndex + gl_VertexID];
	Vertex vertex = vertices[baseVertex + index];
	gl_Position = projection * view * transforms[instance.transform] * vertex.pos;

	worldSpaceNormal = normalize((inverse(transpose(transforms[instance.transform])) * vertex.normal).xyz);
	worldSpaceTangent = normalize((transforms[instance.transform] * vec4(vertex.tangent.xyz, 0.f)).xyz);
}