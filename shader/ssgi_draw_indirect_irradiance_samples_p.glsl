#version 450 core

in VSOut
{
	vec4 color;
} psIn;

out vec4 outColor;

void main()
{
	outColor = vec4(1.f, 0.f, 0.f, 1.f);
}
