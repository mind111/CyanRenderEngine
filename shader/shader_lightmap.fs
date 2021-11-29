#version 450 core
in vec2 worldPos;
out vec4 fragColor;
void main()
{
    fragColor = vec4(vec3(.95f), 1.f);
}