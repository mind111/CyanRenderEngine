#version 450 core

in vec3 voxelColor;

out vec4 fragColor; 

void main()
{
	fragColor = vec4(voxelColor, 1.f);
}
