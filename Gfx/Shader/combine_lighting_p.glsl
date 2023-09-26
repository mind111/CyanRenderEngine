#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

uniform sampler2D u_directLighting;
uniform sampler2D u_indirectLighting;

out vec3 outColor;

void main()
{
	outColor = texture(u_directLighting, psIn.texCoord0).rgb + texture(u_indirectLighting, psIn.texCoord0).rgb;
}
