#version 450 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 tangent;
layout (location = 3) in vec2 texCoord0;
layout (location = 4) in vec2 texCoord1;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out VSOutput
{
	vec2 texCoord0;
} vsOut;

void main() 
{
    vsOut.texCoord0 = texCoord0;
    gl_Position = vec4(position, 1.0f);
}
