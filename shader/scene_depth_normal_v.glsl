#version 450 core

#extension GL_ARB_shader_draw_parameters : enable 
#extension GL_ARB_gpu_shader_int64 : enable 

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

layout(std430, binding = DRAWCALL_BUFFER_BINDING) buffer DrawCallSSBO {
	uint drawCalls[];
};

out vec3 worldSpaceNormal;
out vec3 worldSpaceTangent;

void main() {
	uint instanceIndex = drawCalls[gl_DrawIDARB] + gl_InstanceID;
	InstanceDesc instance = instanceDescs[instanceIndex];
	uint baseVertex = submeshDescs[instance.submesh].baseVertex;
	uint baseIndex = submeshDescs[instance.submesh].baseIndex;
	uint index = indices[baseIndex + gl_VertexID];
	Vertex vertex = vertexBuffer.vertices[baseVertex + index];
	gl_Position = viewSsbo.projection * viewSsbo.view * transformSsbo.models[instance.transform] * vertex.pos;

	worldSpaceNormal = normalize((inverse(transpose(transformSsbo.models[instance.transform])) * vertex.normal).xyz);
	worldSpaceTangent = normalize((transformSsbo.models[instance.transform] * vec4(vertex.tangent.xyz, 0.f)).xyz);
}