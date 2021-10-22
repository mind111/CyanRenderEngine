#version 450 core
in vec3 vn;
layout (location = 1) out float gDepth; 
layout (location = 2) out vec3 gNormal; 
void main()
{
    gDepth = gl_FragCoord.z;
    gNormal = vn * .5f + .5f;
}