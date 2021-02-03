#version 450 core

in vec2 uv;

out vec4 fragColor;

uniform sampler2D quadSampler;

void main()
{
    fragColor= texture(quadSampler, uv);
}