#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec4 outColor;

uniform vec4 u_color;

void main()
{
	outColor = u_color;
}
