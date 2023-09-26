#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outColor;

uniform sampler2D u_sceneAlbedoTex; 
uniform sampler2D u_sceneMetallicRoughnessTex; 
uniform sampler2D u_indirectIrradianceTex; 

void main()
{
	vec3 indirectIrradiance = texture(u_indirectIrradianceTex, psIn.texCoord0).rgb;
	vec3 albedo = texture(u_sceneAlbedoTex, psIn.texCoord0).rgb;
	vec3 MRO = texture(u_sceneMetallicRoughnessTex, psIn.texCoord0).rgb;
	float metallic = MRO.r;
	vec3 diffuseColor = (1.f - metallic) * albedo;
	outColor = indirectIrradiance * diffuseColor;
}
