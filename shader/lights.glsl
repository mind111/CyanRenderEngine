#version 450 core

/**
* All lit material shader should include this file
*/

#define MAX_NUM_POINT_LIGHTS 32

uniform struct DirectionalLight
{
	vec3 direction;
	vec4 colorAndIntensity;
	sampler2D shadowmap;
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
