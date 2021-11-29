#version 450 core
layout (location = 0) in vec3 vPos;
layout (location = 1) in vec2 vLightMapTexCoord;
out vec3 worldPos;
uniform mat4 model;
void main()
{
    vec2 screenCoord = vLightMapTexCoord * 2.f - 1.f;
    worldPos = (model * vec4(vPos, 1.f)).xyz;
    gl_Position = vec4(screenCoord, 0.f, 1.f);
}