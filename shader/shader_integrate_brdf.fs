#version 450 core

in vec2 uv;
out vec4 fragColor;

#define pi 3.14159265359

float hash(float seed)
{
    return fract(sin(seed)*43758.5453);
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
    // TODO: verify this importance sampling ggx procedure
    float theta = atan(roughness * sqrt(rand_u) / sqrt(1 - rand_u));
    float phi = 2 * pi * rand_v;
    return generateSample(n, theta, phi);
}

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

/*
    lambda function in Smith geometry term
*/
float ggxSmithLambda(vec3 v, vec3 h, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float hdotv = saturate(dot(h, v));
    float hdotv2 = max(0.001f, hdotv * hdotv);
    float a2Rcp = alpha2 * (1.f - hdotv2) / hdotv2;
    return 0.5f * (sqrt(1.f + a2Rcp) - 1.f);
}

/*
    * height-correlated Smith geometry attenuation 
*/
float ggxSmithG2(vec3 v, vec3 l, vec3 h, float roughness)
{
    float ggxV = ggxSmithLambda(v, h, roughness);
    float ggxL = ggxSmithLambda(l, h, roughness);
    return 1.f / (1.f + ggxV + ggxL);
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

void main()
{
    float roughness = uv.x;
    float ndotv = uv.y; 
    float sinTheta = sqrt(1 - ndotv * ndotv);
    vec3 v = vec3(sinTheta, ndotv, 0.f);
    uint numSamples = 2048u;
    vec3 n = vec3(0.f, 1.f, 0.f);
    float A = 0.f;
    float B = 0.f;
    for (uint s = 0; s < numSamples; ++s)
    {
        vec2 rand_uv =  HammersleyNoBitOps(s, numSamples);
        vec3 h = importanceSampleGGX(n, roughness, rand_uv.x, rand_uv.y);
        vec3 l = -reflect(v, h);

        float ndotl = dot(n, l);
        float ndoth = saturate(dot(n, h));
        float vdoth = saturate(dot(v, h));

        if (ndotl > 0.f)
        {
            float G = ggxSmithG2(v, l, h, roughness);
            float GVis = G * vdoth / max((ndotv * ndoth), 0.0001f);
            float a = (1.f - vdoth) * (1.f - vdoth) * (1.f - vdoth) * (1.f - vdoth) * (1.f - vdoth);
            A += (1.f - a) * GVis; 
            B += a         * GVis; 
        }
    }
    A /= numSamples;
    B /= numSamples;
    fragColor = vec4(A, B, 0.f, 1.f);
}
