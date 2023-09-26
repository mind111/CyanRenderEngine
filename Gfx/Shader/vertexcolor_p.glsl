#version 450 core

in VertexShaderOutput 
{
	vec4 vertexColor;
} psIn;

out vec4 outColor;

void main()
{
	outColor = psIn.vertexColor;
}
