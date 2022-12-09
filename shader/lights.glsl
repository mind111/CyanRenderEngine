#version 450 core

#extension GL_NV_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable 

const uint kNumShadowCascades = 4;
struct DirectionalShadowMap {
	mat4 lightSpaceView;
	mat4 lightSpaceProjection;
	uint64_t depthTextureHandle;
	vec2 padding;
};

struct Cascade {
	float n;
	float f;
	vec2 padding;
	DirectionalShadowMap shadowMap;
};

struct CascadedShadowMap {
	Cascade cascades[kNumShadowCascades];
};

struct DirectionalLight {
	vec4 colorAndIntensity;
	vec4 direction;
	CascadedShadowMap csm;
};

layout (std430, binding = 8) buffer DirectionalLightBuffer {
	DirectionalLight directionalLights[];
};

uniform struct SkyLight {
	float intensity;
	samplerCube irradiance;
	samplerCube reflection;
} skyLight;
