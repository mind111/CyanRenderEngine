#version 450 core
layout (location = 0) in vec3 vertexPos;
layout(std430, binding = 0) buffer View
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
};

struct InstanceDesc {
    mat4 transform;
    vec4 color;
};

layout(std430, binding = 81) buffer InstanceBuffer {
    InstanceDesc instances[];
};

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};
out VSOut {
    vec3 color;
} vsOut;

void main() {
    InstanceDesc instance = instances[gl_InstanceID];
    mat4 model = instance.transform;
    vsOut.color = instance.color.rgb;
    gl_Position = projection * view * model * vec4(vertexPos, 1.f);
}
