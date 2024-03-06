#version 450 core

in GSOutput
{
	vec3 color;
} psIn;

out vec3 outPixelColor;

void main()
{
	outPixelColor = psIn.color;
}
