#version 450 core

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec4 vTangent;
layout (location = 3) in vec2 vTexCoords;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out VertexData
{
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 texCoords;
    vec3 fragmentWorldPos;
} vsOut;

uniform mat4 model;

void main()
{
    vsOut.position = vPos;
    vsOut.normal = vNormal;
    vsOut.tangent = vTangent;
    vsOut.texCoords = vTexCoords;
    vec4 worldPos = model * vec4(vPos.xyz, 1.0f);
    vsOut.fragmentWorldPos = worldPos.xyz;
    gl_Position = worldPos;
}