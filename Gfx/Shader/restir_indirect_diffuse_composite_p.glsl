#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

layout (location = 0) out vec3 outIndirectIrradiance;
layout (location = 1) out vec3 outIndirectLighting;

uniform sampler2D u_indirectIrradianceTex;
uniform sampler2D u_sceneAlbedoTex;
uniform sampler2D u_sceneDepthTex;
uniform sampler2D u_sceneMetallicRoughnessTex;
uniform float u_indirectIrradianceBoost;

void main()
{
	float depth = texture(u_sceneDepthTex, psIn.texCoord0).r;

    if (depth > 0.999999f) discard;

	vec3 albedo = texture(u_sceneAlbedoTex, psIn.texCoord0).rgb;
	vec3 MRO = texture(u_sceneMetallicRoughnessTex, psIn.texCoord0).rgb;
	vec3 indirectIrradiance = texture(u_indirectIrradianceTex, psIn.texCoord0).rgb * u_indirectIrradianceBoost;
	float metallic = MRO.r;
	float roughness = MRO.g;

	outIndirectIrradiance = indirectIrradiance;
	vec3 diffuseColor = (1.f - metallic) * albedo;
	outIndirectLighting = diffuseColor * indirectIrradiance;
}
