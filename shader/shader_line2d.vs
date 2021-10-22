#version 450 core
in vec3 vPos;
void main()
{
    gl_Position = vec4(vPos, 1.f);
}