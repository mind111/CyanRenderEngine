#version 450 core

uniform sampler2D directLightingTexture;
uniform sampler2D indirectLightingTexture;

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outRadiance;

void main()
{
	outRadiance = texture(directLightingTexture, psIn.texCoord0).rgb + texture(indirectLightingTexture, psIn.texCoord0).rgb;
}
