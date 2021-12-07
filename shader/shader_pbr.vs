#version 450 core

// TODO: No longer hard code the binding in shader, binding can be changed at runtime
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

uniform mat4 s_model;
uniform mat4 s_view;
uniform mat4 s_projection;

void main() {
    fragmentPos = (s_view * s_model * vec4(vertexPos, 1.f)).xyz; 
    fragmentPosWS = (s_model * vec4(vertexPos, 1.f)).xyz;
    gl_Position = s_projection * s_view * s_model * vec4(vertexPos, 1.0f);
    mat4 normalTransformWorld = transpose(inverse(s_model));
    wn = (normalTransformWorld * vec4(vertexNormal, 0.f)).xyz;
    mat4 normalTransform = transpose(inverse(s_view * s_model)); 
    // Transform normals to camera space
    n = (normalTransform * vec4(vertexNormal, 0.f)).xyz;
    n = normalize(n);
    // the w-component of input vertex tangent indicate the handedness of the tangent frame 
    vec3 vt = vertexTangent.xyz * vertexTangent.w;
    // Transform tangents to camera space
    t = (s_view * s_model * vec4(vt, 0.f)).xyz;
    t = normalize(t);
    uv = textureUv_0;
    uv1 = textureUv_1;
}