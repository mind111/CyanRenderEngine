#version 450 core

in vec3 fragmentObjPos;
out vec4 fragColor;

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
}