#version 450 core

out VSOutput
{
    vec4 color;
    vec3 objectSpacePosition;
} vsOut;

struct DebugInstanceData {
    mat4 transform[2];
    vec4 color; 
};

layout(std430, binding = 53) buffer TransformSSBO {
    DebugInstanceData instances[];
};

layout(std430, binding = 0) buffer ViewShaderStorageBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

void main() {
    int vertexIndex = gl_VertexID % 2;
	mat4 model = instances[gl_InstanceID].transform[vertexIndex];
    mat4 mvp = viewSsbo.projection * viewSsbo.view * model;
	gl_Position = mvp * vec4(vec3(0.f), 1.f);
    vsOut.color = instances[gl_InstanceID].color;
}
