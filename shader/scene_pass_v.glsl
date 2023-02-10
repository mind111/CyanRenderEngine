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

#define TextureHandle uint64_t

struct MaterialDesc
{
	TextureHandle albedoMap;
	TextureHandle normalMap;
	TextureHandle metallicRoughnessMap;
	TextureHandle emissiveMap;
    TextureHandle occlusionMap;
	TextureHandle padding;
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

out VSOutput 
{
	vec3 viewSpacePosition;
	vec3 worldSpacePosition;
	vec3 worldSpaceNormal;
	vec3 worldSpaceTangent;
	flat float tangentSpaceHandedness;
	vec2 texCoord0;
	vec2 texCoord1;
	vec3 vertexColor;
	flat MaterialDesc desc;
} vsOut;

void main()
{
	uint instanceIndex = drawCalls[gl_DrawIDARB] + gl_InstanceID;
	InstanceDesc instance = instanceDescs[instanceIndex];
	uint baseVertex = submeshDescs[instance.submesh].baseVertex;
	uint baseIndex = submeshDescs[instance.submesh].baseIndex;
	uint index = indices[baseIndex + gl_VertexID];
	Vertex vertex = vertices[baseVertex + index];
	gl_Position = projection * view * transforms[instance.transform] * vertex.pos;

	vsOut.worldSpacePosition = (transforms[instance.transform] * vertex.pos).xyz;
	vsOut.viewSpacePosition = (view * transforms[instance.transform] * vertex.pos).xyz;
	vsOut.worldSpaceNormal = normalize((inverse(transpose(transforms[instance.transform])) * vertex.normal).xyz);
	vsOut.worldSpaceTangent = normalize((transforms[instance.transform] * vec4(vertex.tangent.xyz, 0.f)).xyz);
	vsOut.tangentSpaceHandedness = vertex.tangent.w;

	vsOut.texCoord0 = vertex.texCoord.xy;
	vsOut.texCoord1 = vertex.texCoord.zw;
	vsOut.vertexColor = vertex.normal.xyz * .5 + .5;
	vsOut.desc = materialDescs[instance.material];
}
