#version 450 core

#define pi 3.14159265359

in vec3 fragmentObjPos;

out vec4 fragColor;

uniform sampler2D rawEnvmapSampler;

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

vec2 sampleEquirectangularMap(vec3 d)
{
    // Cartesian to spherical
    float theta = asin(d.y);
    float phi = atan(d.z, d.x);

    // Spherical to uv
    float u = phi / (2 * pi) + 0.5;
    float v = theta / pi + 0.5; 
    return vec2(u, v);
}

void main()
{
    vec3 d = normalize(fragmentObjPos);
    vec2 uv = sampleEquirectangularMap(d);
    vec3 hdrColor = texture(rawEnvmapSampler, uv).rgb;
    fragColor = vec4(hdrColor, 1.0f);
}