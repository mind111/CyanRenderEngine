#version 450 core

in vec3 fragmentObjPos;
// out vec4 fragColor;
layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec3 fragmentNormal; 
layout (location = 2) out vec3 fragmentDepth; 
uniform samplerCube envmapSampler;

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

void main()
{
    vec3 d = normalize(fragmentObjPos);
    vec3 hdrColor = texture(envmapSampler, d).rgb;
    fragColor = vec4(hdrColor, 1.0f);
    fragmentNormal = vec3(0.f);
    fragmentDepth = vec3(1.f);
}