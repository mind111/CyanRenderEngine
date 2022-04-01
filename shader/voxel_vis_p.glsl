#version 450 core

in vec3 voxelColor;

out vec4 pixelColor;

void main()
{
	pixelColor = vec4(voxelColor, 1.f);
}
