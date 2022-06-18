#version 450 core

/**
* All lit material shader should include this file
*/

#define MAX_NUM_POINT_LIGHTS 32
#define NUM_SHADOW_CASCADES 4

#ifdef VARIANCE_SHADOWMAP
uniform struct VarianceShadowmap
{
	mat4 lightSpaceProjection;
	sampler2D depthTexture;
	sampler2D depthSpuaredTexture;
};

uniform struct Cascade
{
	float n;
	float f;
	VarianceShadowmap shadowmap;
};
#endif

#ifndef VARIANCE_SHADOWMAP
uniform struct DirectionalShadowmap
{
	mat4 lightSpaceProjection;
	sampler2D depthTexture;
};

uniform struct Cascade
{
	float n;
	float f;
	DirectionalShadowmap shadowmap;
};
#endif

uniform struct CascadedShadowmap
{
	Cascade cascades[NUM_SHADOW_CASCADES];
};

uniform struct DirectionalLight
{
	vec3 direction;
	vec4 colorAndIntensity;
    mat4 lightSpaceView;
	CascadedShadowmap csm;
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

