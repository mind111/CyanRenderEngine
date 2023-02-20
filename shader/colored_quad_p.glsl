#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

uniform vec4 color;
out vec4 outColor;

void main()
{
	outColor = color;
}
