#version 450 core

layout (location = 0) in vec3 vertexPos;

layout(std430, binding = 0) buffer ViewBuffer {
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
};

struct InstanceDesc {
	mat4 transform;
	vec4 albedo;
	vec4 radiance;
	vec4 debugColor;
};

layout(std430, binding = 68) buffer InstanceBuffer {
	InstanceDesc instances[];
};

out VSoutput {
	vec3 color;
} vsOut;

#define VIS_ALBEDO 0
#define VIS_RADIANCE 1
#define VIS_DEBUG_COLOR 2
uniform uint visMode;

void main() {
	mat4 model = instances[gl_InstanceID].transform;
	gl_Position = projection * view * model * vec4(vertexPos, 1.f);
	if (visMode == VIS_ALBEDO) {
		vsOut.color = instances[gl_InstanceID].albedo.rgb;
	}
	else if (visMode == VIS_RADIANCE) {
		vsOut.color = instances[gl_InstanceID].radiance.rgb;
	}
	else if (visMode == VIS_DEBUG_COLOR) {
		vsOut.color = instances[gl_InstanceID].debugColor.rgb;
	}
}
