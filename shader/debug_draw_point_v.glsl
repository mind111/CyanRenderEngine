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

layout(std430) buffer ViewBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
};

void main() 
{
	gl_Position = projection * view * vertices[gl_VertexID].position;
    gl_PointSize = 20.f;
    vsOut.color = vertices[gl_VertexID].color;
}
