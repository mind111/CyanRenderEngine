#version 450 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec4 vertexTangent;
layout (location = 3) in vec2 textureUv_0;
layout (location = 4) in vec2 textureUv_1;
layout (location = 5) in vec2 textureUv_2;
layout (location = 6) in vec2 textureUv_3;

out vec3 n;
out vec3 wn;
out vec3 t;
out vec2 uv;
out vec2 uv1;
out vec4 shadowPos;
out vec3 fragmentPos;
out vec3 fragmentPosWS;

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

void main() {
    mat4 model = transformSsbo.models[transformIndex];
    fragmentPos = (viewSsbo.view * model * vec4(vertexPos, 1.f)).xyz; 
    fragmentPosWS = (model * vec4(vertexPos, 1.f)).xyz;
    gl_Position = viewSsbo.projection * viewSsbo.view * model * vec4(vertexPos, 1.0f);
    mat4 normalTransformWorld = transpose(inverse(model));
    wn = (normalTransformWorld * vec4(vertexNormal, 0.f)).xyz;
    mat4 normalTransform = transpose(inverse(viewSsbo.view * model)); 
    // Transform normals to camera space
    n = (normalTransform * vec4(vertexNormal, 0.f)).xyz;
    n = normalize(n);
    // the w-component of input vertex tangent indicate the handedness of the tangent frame 
    vec3 vt = vertexTangent.xyz * vertexTangent.w;
    // Transform tangents to camera space
    t = (viewSsbo.view * model * vec4(vt, 0.f)).xyz;
    t = normalize(t);
    uv = textureUv_0;
    uv1 = textureUv_1;
}