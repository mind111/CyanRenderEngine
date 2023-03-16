#version 450 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 tangent;
layout (location = 3) in vec2 texCoord0;
layout (location = 4) in vec2 texCoord1;

/**
* scene shader storage buffers
*/

layout(std430) buffer ViewBuffer 
{
    mat4  view;
    mat4  projection;
};

layout(std430) buffer TransformBuffer 
{
    mat4 transforms[];
};

struct Vertex 
{
	vec4 pos;
	vec4 normal;
	vec4 tangent;
	vec4 texCoord;
};

uniform int instanceID;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out VSOutput 
{
	vec3 viewSpacePosition;
	vec3 worldSpacePosition;
	vec3 worldSpaceNormal;
	vec3 worldSpaceTangent;
	flat float tangentSpaceHandedness;
	vec2 texCoord0;
	vec2 texCoord1;
	vec3 vertexColor;
} vsOut;

void main()
{
	Vertex vertex;
	vertex.pos = vec4(position, 1.f);
	vertex.normal = vec4(normal, 0.f);
	vertex.tangent = tangent;
	vertex.texCoord = vec4(texCoord0, texCoord1);

	gl_Position = projection * view * transforms[instanceID] * vertex.pos;

	vsOut.worldSpacePosition = (transforms[instanceID] * vertex.pos).xyz;
	vsOut.viewSpacePosition = (view * transforms[instanceID] * vertex.pos).xyz;
	vsOut.worldSpaceNormal = normalize((inverse(transpose(transforms[instanceID])) * vertex.normal).xyz);
	vsOut.worldSpaceTangent = normalize((transforms[instanceID] * vec4(vertex.tangent.xyz, 0.f)).xyz);
	vsOut.tangentSpaceHandedness = vertex.tangent.w;

	vsOut.texCoord0 = vertex.texCoord.xy;
	vsOut.texCoord1 = vertex.texCoord.zw;
	vsOut.vertexColor = vertex.normal.xyz * .5 + .5;
}