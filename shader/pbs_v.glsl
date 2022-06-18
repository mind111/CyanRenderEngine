#version 450 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec4 vertexTangent;
layout (location = 3) in vec2 textureUv_0;
layout (location = 4) in vec2 textureUv_1;
layout (location = 5) in vec2 textureUv_2;
layout (location = 6) in vec2 textureUv_3;

out vec3 viewSpacePosition;
out vec3 worldSpacePosition;
out vec3 worldSpaceNormal;
out vec3 viewSpaceTangent;
out vec2 texCoord0;
out vec2 texCoord1;

#define VIEW_SSBO_BINDING 0
#define TRANSFORM_SSBO_BINDING 3

layout(std430, binding = VIEW_SSBO_BINDING) buffer ViewShaderStorageBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

layout(std430, binding = TRANSFORM_SSBO_BINDING) buffer TransformShaderStorageBuffer
{
    mat4 models[];
} transformSsbo;

uniform int transformIndex;

vec3 transformNormal(in vec3 normal, mat4 transform)
{
    vec3 result = (transpose(inverse(transform)) * vec4(normal, 0.f)).xyz;
    return normalize(result);
}

void main() {
    mat4 model = transformSsbo.models[transformIndex];

    viewSpacePosition = (viewSsbo.view * model * vec4(vertexPos, 1.f)).xyz; 
    worldSpacePosition = (model * vec4(vertexPos, 1.f)).xyz;
    gl_Position = viewSsbo.projection * viewSsbo.view * model * vec4(vertexPos, 1.0f);

    worldSpaceNormal = transformNormal(vertexNormal, model);

    // the w-component of input vertex tangent indicate the handedness of the tangent frame 
    vec3 tangent = vertexTangent.xyz * vertexTangent.w;
    // Transform tangents to camera space
    viewSpaceTangent = normalize((viewSsbo.view * model * vec4(tangent, 0.f)).xyz);

    texCoord0 = textureUv_0;
    texCoord1 = textureUv_1;
}