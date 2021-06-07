
#version 450 core

#define pi 3.14159265359

in vec3 fragmentObjPos;

out vec4 fragColor;

uniform float roughness;
uniform float drawSamples;

// HDR envmap
uniform samplerCube envmapSampler;

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

vec3 generateSample(vec3 n, float theta, float phi)
{
    float x = sin(theta) * sin(phi);
    float y = sin(theta) * cos(phi);
    float z = cos(theta);
    vec3 s = vec3(x, y, z);
    // Prevent the case where n is  (0.f, 1.f, 0.f)
    vec3 up = abs(n.x) > 0.f ? vec3(0.f, 1.f, 0.f) : vec3(0.f, 0.f, 1.f);
    vec3 xAxis = cross(up, n);
    vec3 yAxis = cross(n, xAxis);
    vec3 zAxis = n;
    mat3 toWorldSpace = {
        xAxis,
        yAxis,
        zAxis
    };
    return normalize(toWorldSpace * s);
}

float hash(float seed)
{
    return fract(sin(seed)*43758.5453);
}

vec3 importanceSampleGGX(vec3 n, float roughness, float rand_u, float rand_v)
{
    // TODO: verify this importance sampling ggx procedure
    float theta = atan(roughness * sqrt(rand_u) / sqrt(1 - rand_u));
    float phi = 2 * pi * rand_v;
    return generateSample(n, theta, phi);
}

// Watchout for padding if using vec3
layout(std430, binding = 3) buffer sampleVertexData 
{
    vec4 vertices[];
} sampleVertexBuffer;

void writeSampleDirectionData()
{
    int numSamples = 8;
    // generate samples aroud the hemisphere oriented by a fix normal direction
    vec3 fixedNormal = normalize(vec3(-0.2f, -0.5f, 0.3f));
    for (int s = 0; s < numSamples; s++)
    {
        float rand_u = hash(s * 12.3f / numSamples); 
        float rand_v = hash(s * 78.2f / numSamples); 
        vec3 h = importanceSampleGGX(fixedNormal, roughness, rand_u, rand_v);
        sampleVertexBuffer.vertices[s * 2] = vec4(0.f, 0.f, -1.5f, 1.f);
        sampleVertexBuffer.vertices[s * 2 + 1] = vec4(h + vec3(0.f, 0.f, -1.5f), 1.f);
    }
    // Draw a line for the normal
    sampleVertexBuffer.vertices[numSamples * 2] = vec4(0.f, 0.f, -1.5f, 1.f);
    sampleVertexBuffer.vertices[numSamples * 2 + 1] = vec4(fixedNormal + vec3(0.f, 0.f, -1.5f), 1.f);
}

void main()
{
    vec3 n = normalize(fragmentObjPos);
    // fix viewDir
    vec3 viewDir = n;
    uint numSamples = 1024;
    vec3 result = vec3(0.f);
    float weight = 0.f; 
    for (int s = 0; s < numSamples; s++)
    {
        float rand_u = hash(s * 12.3f / numSamples); 
        float rand_v = hash(s * 78.2f / numSamples); 
        vec3 h = importanceSampleGGX(n, roughness, rand_u, rand_v);
        vec3 l = -reflect(viewDir, h);
        // why not use microfacet normal h here...?
        float ndotl = dot(n, l);
        if (ndotl > 0)
        {
            result += texture(envmapSampler, l).rgb * ndotl;
            weight += ndotl;
        }
    }
    if (drawSamples > .5f)
    {
        writeSampleDirectionData();
    }
    // according to Epic's notes, weighted by ndotl produce better visual results
    result /= weight; 
    fragColor = vec4(result, 1.f);
}