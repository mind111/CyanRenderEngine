#version 450 core

#extension GL_NV_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable 

/**
* Global shader storage buffers & uniform buffer
*/

layout(std430) buffer ViewBuffer {
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
};

layout(std430) buffer TransformBuffer {
    mat4 transforms[];
};

struct Vertex {
	vec4 pos;
	vec4 normal;
	vec4 tangent;
	vec4 texCoord;
};

layout(std430) buffer VertexBuffer {
	Vertex vertices[];
};

layout(std430) buffer IndexBuffer {
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
