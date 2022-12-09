#version 450 core

#extension GL_NV_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable 

/**
* Global shader storage buffers & uniform buffer
*/

#define VIEW_BUFFER_BINDING 0
#define TRANSFORM_BUFFER_BINDING 1
#define INSTANCE_DESC_BUFFER_BINDING 2
#define SUBMESH_BUFFER_BINDING 3
#define VERTEX_BUFFER_BINDING 4
#define INDEX_BUFFER_BINDING 5
#define DRAWCALL_BUFFER_BINDING 6
#define MATERIAL_BUFFER_BINDING 7
#define DIRECTIONALLIGHT_BUFFER_BINDING 8

layout(std430, binding = VIEW_BUFFER_BINDING) buffer ViewBuffer {
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

layout(std430, binding = TRANSFORM_BUFFER_BINDING) buffer TransformBuffer {
    mat4 models[];
} transformSsbo;

struct Vertex {
	vec4 pos;
	vec4 normal;
	vec4 tangent;
	vec4 texCoord;
};

layout(std430, binding = VERTEX_BUFFER_BINDING) buffer VertexBuffer {
	Vertex vertices[];
} vertexBuffer;

layout(std430, binding = INDEX_BUFFER_BINDING) buffer IndexBuffer {
	uint indices[];
};

struct InstanceDesc {
	uint submesh;
	uint material;
	uint transform;
	uint padding;
};

layout(std430, binding = INSTANCE_DESC_BUFFER_BINDING) buffer InstanceSSBO {
	InstanceDesc instanceDescs[];
};

struct SubmeshDesc {
	uint baseVertex;
	uint baseIndex;
	uint numVertices;
	uint numIndices;
};

layout(std430, binding = SUBMESH_BUFFER_BINDING) buffer SubmeshSSBO {
	SubmeshDesc submeshDescs[];
};

struct PBRMaterial {
	uint64_t diffuseMap;
	uint64_t normalMap;
	uint64_t metallicRoughnessMap;
	uint64_t occlusionMap;
	vec4 kAlbedo;
	vec4 kMetallicRoughness;
	uvec4 flags;
};

layout(std430, binding = MATERIAL_BUFFER_BINDING) buffer MaterialSSBO {
	PBRMaterial materials[];
};

layout(std430, binding = DRAWCALL_BUFFER_BINDING) buffer DrawCallSSBO {
	uint drawCalls[];
};
