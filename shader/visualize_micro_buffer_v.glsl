#version 450 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec4 vertexTangent;
layout (location = 3) in vec2 textureUv_0;

out VSOut {
	vec2 texCoord;
} vsOut;

void main() {
    vsOut.texCoord = textureUv_0;
    gl_Position = vec4(vertexPos, 1.0f);
}
