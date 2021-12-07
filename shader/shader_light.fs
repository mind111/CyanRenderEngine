#version 450 core

out vec4 fragColor;
uniform vec4 color;

void main()
{
    fragColor = vec4(color.rgb * color.a, 1.f);
}