#version 450 core

in vec2 uv;

uniform sampler2D srcTexture;

out vec4 fragColor;

void main()
{
    fragColor = vec4(texture(srcTexture, uv).rgb, 1.0f);
}