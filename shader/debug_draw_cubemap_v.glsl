#version 450 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec4 vertexTangent;
layout (location = 3) in vec2 textureUv_0;
layout (location = 4) in vec2 textureUv_1;

out VSOutput
{
	vec3 objectSpacePosition;
} vsOut;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    mat4 mvp = projection * view * model;
    gl_Position = mvp * vec4(vertexPos, 1.f);
    vsOut.objectSpacePosition = vertexPos;
}
