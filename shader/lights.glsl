#version 450 core

/**
* All lit material shader should include this file
*/

#define MAX_NUM_POINT_LIGHTS 32
#define NUM_SHADOW_CASCADES 4

uniform struct DirectionalShadowmap
{
	mat4 lightSpaceProjection;
	sampler2D depthTexture;
};

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
	DirectionalShadowmap shadowmap;
};

uniform struct CascadedShadowmap
{
	Cascade cascades[NUM_SHADOW_CASCADES];
};

uniform struct DirectionalLight
{
	vec3 direction;
	vec4 colorAndIntensity;
	CascadedShadowmap csm;
};

uniform struct PointLight
{
	vec3 position;
	vec4 colorAndIntensity;
	samplerCube shadowmap;
};

uniform struct SceneLights
{
	DirectionalLight directionalLight;
	PointLight pointLights[MAX_NUM_POINT_LIGHTS];
} sceneLights;


