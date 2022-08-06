#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec4 outColor;

void main() 
{
	outColor = vec4(1.f, 1.f, 0.f, 1.f);
}
