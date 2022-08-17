#version 450 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec4 vertexTangent;
layout (location = 3) in vec2 textureUv_0;
layout (location = 4) in vec2 textureUv_1;
layout (location = 5) in vec2 textureUv_2;
layout (location = 6) in vec2 textureUv_3;

out VSOutput
{
	vec2 texCoord0;
} vsOut;

void main() {
    gl_Position = vec4(vertexPos, 1.0f);
    vsOut.texCoord0 = textureUv_0;
}