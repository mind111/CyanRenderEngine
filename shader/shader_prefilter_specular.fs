
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

float VanDerCorput(uint n, uint base)
{
    float invBase = 1.0 / float(base);
    float denom   = 1.0;
    float result  = 0.0;
    for(uint i = 0u; i < 32u; ++i)
    {
        if(n > 0u)
        {
            denom   = mod(float(n), 2.0);
            result += denom * invBase;
            invBase = invBase / 2.0;
            n       = uint(float(n) / 2.0);
        }
    }
    return result;
}

vec2 HammersleyNoBitOps(uint i, uint N)
{
    return vec2(float(i)/float(N), VanDerCorput(i, 2u));
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

vec3 importanceSampleGGX(vec3 n, float roughness, float rand_u, float rand_v)
{
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
        vec2 uv = HammersleyNoBitOps(s, numSamples);
        vec3 h = importanceSampleGGX(fixedNormal, roughness, uv.x, uv.y);
        sampleVertexBuffer.vertices[s * 2] = vec4(0.f, 0.f, -1.5f, 1.f);
        sampleVertexBuffer.vertices[s * 2 + 1] = vec4(h + vec3(0.f, 0.f, -1.5f), 1.f);
    }
    // Draw a line for the normal
    sampleVertexBuffer.vertices[numSamples * 2] = vec4(0.f, 0.f, -1.5f, 1.f);
    sampleVertexBuffer.vertices[numSamples * 2 + 1] = vec4(fixedNormal + vec3(0.f, 0.f, -1.5f), 1.f);
}

float GGX(float roughness, float ndoth) 
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float result = alpha2;
    float denom = ndoth * ndoth * (alpha2 - 1.f) + 1.f;
    result /= (pi * denom * denom); 
    return result;
}

void main()
{
    vec3 n = normalize(fragmentObjPos);
    // fix viewDir
    vec3 viewDir = n;
    uint numSamples = 2048;
    vec3 result = vec3(0.f);
    float weight = 0.f; 
    for (int s = 0; s < numSamples; s++)
    {
        vec2 uv = HammersleyNoBitOps(s, numSamples);
        vec3 h = importanceSampleGGX(n, roughness, uv.x, uv.y);
        vec3 l = -reflect(viewDir, h);
        // why not use microfacet normal h here...?
        float ndotl = dot(n, l);
        float ndoth = saturate(dot(n, h));
        float vdoth = saturate(dot(h, viewDir));
        if (ndotl > 0)
        {
            /*
                Note: 
                    compute the importance of current sample, if the importance for 
                current sample is small, which means more texels should be averaged,
                which is equivalent to picking a texel from a lower mipmap level.
            */
            float D = GGX(roughness, ndoth);
            float pdf = (D * ndoth) / (4 * vdoth + 0.0001f);
            float resolution = 1024.0; // resolution of source cubemap (per face)
            float texelSolidAngle  = 4.0 * pi / (6.0 * resolution * resolution);
            float sampleSolidAngle = 1.0 / (float(numSamples) * pdf + 0.0001);
            float mipLevel = roughness == 0.0 ? 0.0 : 0.5 * log2(sampleSolidAngle / texelSolidAngle); 

            result += textureLod(envmapSampler, l, mipLevel).rgb * ndotl;
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