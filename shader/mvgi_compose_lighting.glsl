#version 450 core

uniform sampler2D mvgiDirectLighting;
uniform sampler2D mvgiIndirectLighting;

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outColor;

void main()
{
	outColor = texture(mvgiDirectLighting, psIn.texCoord0).rgb + texture(mvgiIndirectLighting, psIn.texCoord0).rgb;
}