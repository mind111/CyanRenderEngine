#version 450 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec4 vertexTangent;
layout (location = 3) in vec2 textureUv_0;
layout (location = 4) in vec2 textureUv_1;
layout (location = 5) in vec2 textureUv_2;
layout (location = 6) in vec2 textureUv_3;

#define TRANSFORM_SSBO_BINDING 3
layout(std430, binding = TRANSFORM_SSBO_BINDING) buffer TransformShaderStorageBuffer
{
    mat4 models[];
} transformSsbo;

out vec3 worldSpacePosition;

uniform int transformIndex; 
uniform mat4 view;
uniform mat4 projection;

void main() {
    mat4 model = transformSsbo.models[transformIndex];
    worldSpacePosition = (model * vec4(vertexPos, 1.f)).xyz;
    gl_Position = projection * view * model * vec4(vertexPos, 1.f);
}
