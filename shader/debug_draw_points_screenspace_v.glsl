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
	gl_Position = vertices[gl_VertexID].position;
    gl_PointSize = 10.f; // hardcoded for now
    vsOut.color = vertices[gl_VertexID].color;
}
