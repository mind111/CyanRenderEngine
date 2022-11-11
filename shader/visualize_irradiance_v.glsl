#version 450 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec4 vertexTangent;
layout (location = 3) in vec2 textureUv_0;
layout (location = 4) in vec2 textureUv_1;

out VSOutput {
	vec3 color;
} vsOut;

layout(std430, binding = 0) buffer ViewBuffer {
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
};

layout(std430, binding = 66) buffer IrradianceCoordBuffer {
	vec2 irradianceCoords[];
};

layout(std430, binding = 67) buffer InstanceBuffer {
	mat4 transforms[];
};

uniform sampler2D irradianceBuffer;

void main() {
    mat4 model = transforms[gl_InstanceID];
    gl_Position = projection * view * model * vec4(vertexPos, 1.f);
    vsOut.color = texture(irradianceBuffer, irradianceCoords[gl_InstanceID]).rgb;
}
