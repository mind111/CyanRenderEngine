#version 450 core

/**
* All lit material shader should include this file
*/

const uint kNumShadowCascades = 4;
uniform struct DirectionalShadowmap {
	mat4 lightSpaceProjection;
	uint64_t depthTextureHandle;
	vec2 padding;
};

uniform struct Cascade {
	float n;
	float f;
	vec2 padding;
	DirectionalShadowmap shadowmap;
};

uniform struct CascadedShadowMap {
	Cascade cascades[kNumShadowCascades];
};

uniform struct DirectionalLight {
	vec4 direction;
	vec4 colorAndIntensity;
    mat4 lightSpaceView;
	CascadedShadowMap csm;
};

layout (std430, binding = 2) buffer DirectionalLightBuffer {

};

uniform struct PointLight
{
	vec3 position;
	vec4 colorAndIntensity;
	samplerCube shadowmap;
};

uniform struct SkyLight
{
	samplerCube irradiance;
	samplerCube reflection;
};

uniform struct SceneLights
{
	SkyLight skyLight;
	DirectionalLight directionalLight;
	PointLight pointLights[MAX_NUM_POINT_LIGHTS];
} sceneLights;

