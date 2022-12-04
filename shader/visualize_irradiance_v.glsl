#version 450 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec4 vertexTangent;

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

struct InstanceDesc {
    mat4 transform;
    ivec2 atlasTexCoord;
    ivec2 padding;
};

layout(std430, binding = 63) buffer InstanceBuffer {
    InstanceDesc instances[];
};

uniform sampler2D irradianceBuffer;

void main() {
    mat4 model = instances[gl_InstanceID].transform;
    gl_Position = projection * view * model * vec4(vertexPos, 1.f);
    vsOut.color = texture(irradianceBuffer, vec2(instances[gl_InstanceID].atlasTexCoord) / textureSize(irradianceBuffer, 0)).rgb;
}
