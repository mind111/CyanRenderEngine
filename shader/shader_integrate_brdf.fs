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

float G(vec3 v, vec3 n, vec3 h, float roughness)
{
    float ndotv = dot(n,v);
    float hdotv = dot(h,v);
    if ((hdotv / ndotv) <= 0.f) return 0.f;
    float ndotv2 = ndotv * ndotv;
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float tanTheta2 = (1.f - ndotv2) / ndotv2;
    float result = 2.f / (1.f + sqrt(1.f + alpha2 * tanTheta2));
    return result;
}

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

void main()
{
    float ndotv = uv.x; 
    float roughness = uv.y;
    float sinTheta = sqrt(1 - ndotv * ndotv);
    vec3 v = vec3(sinTheta, ndotv, 0.f);
    int numSamples = 1024;
    vec3 n = vec3(0.f, 1.f, 0.f);
    float A = 0.f;
    float B = 0.f;
    for (int s = 0; s < numSamples; ++s)
    {
        float rand_u = hash(s * 12.3f / numSamples); 
        float rand_v = hash(s * 78.2f / numSamples); 
        vec3 h = importanceSampleGGX(n, roughness, rand_u, rand_v);
        vec3 l = -reflect(v, h);

        float ndotl = dot(n, l);
        float ndoth = saturate(dot(n, h));
        float vdoth = saturate(dot(v, h));

        if (ndotl > 0.f)
        {
            float G = G(l, n, h, roughness) * G(v, n, h, roughness);
            float a = (1.f - vdoth) * (1.f - vdoth) * (1.f - vdoth) * (1.f - vdoth) * (1.f - vdoth);
            A += (1.f - a) * G * vdoth / max((ndotv * ndoth), 0.0001f); 
            B += a         * G * vdoth / max((ndotv * ndoth), 0.0001f); 
        }
    }
    A /= numSamples;
    B /= numSamples;
    fragColor = vec4(A, B, 0.f, 1.f);
}
