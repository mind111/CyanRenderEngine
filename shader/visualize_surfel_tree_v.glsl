#version 450 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec4 vertexTangent;
layout (location = 3) in vec2 textureUv_0;
layout (location = 4) in vec2 textureUv_1;

struct InstanceDesc {
	mat4 transform;
	vec4 albedo;
	vec4 radiance;
	vec4 debugColor;
};

layout (std430, binding = 72) buffer InstanceBuffer {
	InstanceDesc instances[];
};

layout(std430, binding = 0) buffer ViewBuffer {
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
};

out VSOutput {
	vec3 color;
} vsOut;

#define VIS_BOUNDING_SPHERE 0
#define VIS_ALBEDO 1
#define VIS_RADIANCE 2
uniform uint visMode;

void main() {
	InstanceDesc instance = instances[gl_InstanceID];
	mat4 model = instance.transform;
	gl_Position = projection * view * model * vec4(vertexPos, 1.f);
	if (visMode == VIS_BOUNDING_SPHERE) {
		vsOut.color = instance.debugColor.rgb;
	}
	else if (visMode == VIS_ALBEDO) {
		vsOut.color = instance.albedo.rgb;
	}
	else if (visMode == VIS_RADIANCE) {

	}
}
