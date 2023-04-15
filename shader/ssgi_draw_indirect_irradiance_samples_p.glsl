#version 450 core

in VSOut
{
	vec4 color;
} psIn;

out vec4 outColor;

void main()
{
	outColor = vec4(psIn.color.rgb, 1.f);
}
