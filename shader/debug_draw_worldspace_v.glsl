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

uniform mat4 cameraView;
uniform mat4 cameraProjection;

void main() 
{
	gl_Position = cameraProjection * cameraView * vertices[gl_VertexID].position;
    vsOut.color = vertices[gl_VertexID].color;
}
