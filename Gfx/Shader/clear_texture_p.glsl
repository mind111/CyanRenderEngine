#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec4 outColor;

uniform vec4 u_clearColor;

void main()
{
    outColor = vec4(u_clearColor);
}