#version 450 core

layout(location = 0) out vec3 outAlbedo;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outMetallicRoughness;

in VertexShaderOutput 
{
	vec3 viewSpacePosition;
	vec3 worldSpacePosition;
	vec3 worldSpaceNormal;
	vec3 worldSpaceTangent;
	flat float tangentSpaceHandedness;
	vec2 texCoord0;
	vec2 texCoord1;
	vec3 vertexColor;
} psIn;

void main()
{
	outAlbedo = vec3(psIn.worldSpaceNormal * .5f + .5f);
	outNormal = psIn.worldSpaceNormal;
	outMetallicRoughness = vec3(0.f, .5f, 0.f);
}
