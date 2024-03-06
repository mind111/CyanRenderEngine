#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outColor;

uniform sampler2D u_indirectLighting;

void main()
{
	outColor = texture(u_indirectLighting, psIn.texCoord0).rgb;
}