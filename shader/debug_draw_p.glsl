#version 450 core

out vec3 outColor;

in VSOutput
{
    vec4 color;
} psIn;

void main() 
{
	outColor = psIn.color.rgb;
}
