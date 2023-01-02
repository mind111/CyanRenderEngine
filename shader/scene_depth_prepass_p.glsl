#version 450 core

out vec3 depth;

void main()
{
	depth = vec3(gl_FragCoord.z);
}
