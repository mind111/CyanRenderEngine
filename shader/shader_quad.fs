#version 450 core

in vec2 uv;
out vec4 fragColor;

uniform vec3 color;

void main()
{
    fragColor = vec4(color, 1.f);
    // fragColor = vec4(0.4f, 0.5f, 0.9f, 1.f);
}