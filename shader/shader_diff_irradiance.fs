#version 450 core

#define pi 3.14159265359

in vec3 fragmentObjPos;

out vec4 fragColor;

// HDR envmap
uniform samplerCube envmapSampler;

layout(std430, binding = 3) buffer sampleVertexData 
{
    vec3 vertices[];
} sampleVertexBuffer;

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

vec3 generateSample(vec3 n, float theta, float phi)
{
    float x = sin(theta) * cos(phi);
    float y = sin(theta) * sin(phi);
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
    return toWorldSpace * s;
}

// No need for tonemapping at the end as the result will be used in lighting instead of being
// displayed by the framebuffer
void main()
{
    vec3 n = normalize(fragmentObjPos);
    float numSamples = 8;
    vec3 result = vec3(0.f);
    float deltaTheta = 0.5f * pi / numSamples;
    float deltaPhi = 2.f * pi / numSamples;

    // Draw debugging lines 
    {
        for (int s = 0; s < 4; s++)
        {
            float theta = s * deltaTheta;
            for (int t = 0; t < 4; t++)
            {
                float phi = t * deltaPhi;
                vec3 v = generateSample(vec3(0.f, 1.f, 0.f), theta, phi);
                sampleVertexBuffer.vertices[s * 4 + t] = v;
            }
        }
    }
    
    for (int s = 0; s < numSamples; s++)
    {
        float theta = s * deltaTheta;
        for (int t = 0; t < numSamples; t++)
        {
            float phi = t * deltaPhi;
            vec3 r = generateSample(n, theta, phi);
            result += texture(envmapSampler, r).rgb;
        }
    }

    result = result / (numSamples * numSamples);
    fragColor = vec4(result, 1.f);
}