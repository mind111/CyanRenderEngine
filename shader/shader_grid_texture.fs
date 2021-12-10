#version 450 core
in vec3 worldPos;
out vec4 fragColor;
#define EPSILON 0.01

void main()
{
    vec3 color = abs(x - round(x)) < EPSILON ? vec3(.2f) : vec3(1.f);
    fragColor = vec4(color, 1.f);
}