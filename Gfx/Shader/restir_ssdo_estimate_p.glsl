#version 450 core

#define PI 3.1415926

in VSOutput
{
	vec2 texCoord0;
} psIn;

layout (location = 0) out vec3 outSkyIrradiance;
layout (location = 1) out vec3 outDirectLighting;
layout (location = 2) out vec3 outDirectDiffuseLighting;

struct ViewParameters
{
	uvec2 renderResolution;
	float aspectRatio;
	mat4 viewMatrix;
    mat4 prevFrameViewMatrix;
	mat4 projectionMatrix;
    mat4 prevFrameProjectionMatrix;
	vec3 cameraPosition;
	vec3 prevFrameCameraPosition;
	vec3 cameraRight;
	vec3 cameraForward;
	vec3 cameraUp;
	int frameCount;
	float elapsedTime;
	float deltaTime;
};

uniform ViewParameters viewParameters;
uniform sampler2D u_sceneDepthTex; 
uniform sampler2D u_sceneNormalTex; 
uniform sampler2D u_sceneAlbedoTex; 
uniform sampler2D u_sceneMetallicRoughnessTex; 
uniform sampler2D u_reservoirSkyRadiance;
uniform sampler2D u_reservoirSkyDirection;
uniform sampler2D u_reservoirSkyWSumMW;
uniform float u_applyAO;
uniform sampler2D u_SSAOTex;

void main()
{
	float depth = texture(u_sceneDepthTex, psIn.texCoord0).r; 
	vec3 n = texture(u_sceneNormalTex, psIn.texCoord0).rgb * 2.f - 1.f;

    if (depth > 0.999999f) discard;

	vec3 albedo = texture(u_sceneAlbedoTex, psIn.texCoord0).rgb;
	vec3 MRO = texture(u_sceneMetallicRoughnessTex, psIn.texCoord0).rgb;
	float metallic = MRO.r;
	float roughness = MRO.g;
	float ao = u_applyAO > 0.5f ? texture(u_SSAOTex, psIn.texCoord0).r : 1.f;

	vec3 reservoirRadiance = texture(u_reservoirSkyRadiance, psIn.texCoord0).rgb;
	vec3 reservoirDirection = texture(u_reservoirSkyDirection, psIn.texCoord0).rgb;
	vec3 reservoirWSumMW = texture(u_reservoirSkyWSumMW, psIn.texCoord0).rgb;

	vec3 skyIrradianceEstimate = reservoirRadiance * max(dot(n, reservoirDirection), 0.f) * reservoirWSumMW.z * ao; 
    outSkyIrradiance = skyIrradianceEstimate;

	vec3 diffuseColor = (1.f - metallic) * albedo;
	// note that no need to include the ndotl term here as the estimate already take that term into estimation
	// when calculating each samples target pdf
    outDirectLighting = skyIrradianceEstimate * diffuseColor;
    outDirectDiffuseLighting = skyIrradianceEstimate * diffuseColor;
}
