#version 450 core

in GS_OUT 
{
	vec3 color;
} psIn;

out vec4 fragColor;

void main()
{
	fragColor = vec4(psIn.color, 1.f);
}
