#version 450 core

in vec3 fragmentObjPos;
out vec4 fragColor;

uniform samplerCube envmapSampler;

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    vec3 result = (x*(a*x+b))/(x*(c*x+d)+e);
    return vec3(saturate(result.r), saturate(result.g), saturate(result.b)); 
}

void main()
{
    vec3 d = normalize(fragmentObjPos);
    vec3 hdrColor = texture(envmapSampler, d).rgb;
    vec3 result = ACESFilm(hdrColor);
    float gamma = 1.f / 2.2f;
    result = vec3(pow(result.r, gamma), pow(result.g, gamma), pow(result.b, gamma));
    fragColor = vec4(result, 1.0f);
}