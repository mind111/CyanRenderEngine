#version 450 core
layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec4 vertexTangent;
layout (location = 3) in vec2 textureUv_0;
layout (location = 4) in vec2 lightMapTexCoord; // light map uv
layout (location = 5) in vec2 textureUv_2;
layout (location = 6) in vec2 textureUv_3;

out vec3 worldPos;
out vec3 normal;

uniform mat4 model;
uniform vec3 uvOffset;

void main()
{
    vec2 screenCoord = (lightMapTexCoord + uvOffset.xy) * 2.f - 1.f;
    worldPos = (model * vec4(vertexPos, 1.f)).xyz;
    normal = (inverse(transpose(model)) * vec4(vertexNormal, 0.f)).xyz;
    normal = normalize(normal);
    gl_Position = vec4(screenCoord, 0.f, 1.f);
}