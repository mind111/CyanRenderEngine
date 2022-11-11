#version 450 core
layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexNormal;

struct InstanceDesc {
	mat4 transform;
    vec4 color;
};

layout(std430, binding = 0) buffer View
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
};

layout (std430, binding = 80) buffer InstanceBuffer {
	InstanceDesc instances[];
};

out VSOut { 
    vec3 color;
} vsOut;

void main() { 
    mat4 model = instances[gl_InstanceID].transform;
    gl_Position = projection * view * model * vec4(vertexPos, 1.f);
    vsOut.color = instances[gl_InstanceID].color.rgb;
}
