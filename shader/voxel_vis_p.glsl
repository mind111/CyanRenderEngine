#version 450 core

in vec4 voxelColor;

out vec4 pixelColor;

void main()
{
	pixelColor = vec4(voxelColor);
}
