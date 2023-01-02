#version 450 core

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out VSOutput
{
    vec4 color;
} vsOut;

struct Vertex
{
    vec4 position;
    vec4 color;
};

layout(std430) buffer VertexBuffer 
{
    Vertex vertices[];
};

void main() 
{
    int vertexIndex = gl_VertexID / 2 + gl_VertexID % 2;
	gl_Position = vertices[vertexIndex].position;
    vsOut.color = vertices[vertexIndex].color;
}
