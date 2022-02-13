#version 450 core

#define pi 3.14159265359

in vec3 fragmentObjPos;

out vec4 fragColor;

// HDR envmap
uniform samplerCube srcCubemapTexture;

// Watchout for padding if using vec3
layout(std430, binding = 3) buffer sampleVertexData 
{
    vec4 vertices[];
} sampleVertexBuffer;

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

void writeSampleDirectionData()
{
    int numSamples = 8;
    float deltaTheta = 0.5f * pi / numSamples;
    float deltaPhi = 2.f * pi / numSamples;
    // generate samples aroud the hemisphere oriented by a fix normal direction
    vec3 normal = normalize(vec3(-0.2f, -0.5f, 0.3f));
    for (int s = 0; s < 8; s++)
    {
        float theta = pi / 2.f - s * deltaTheta;
        for (int t = 0; t < 8; t++)
        {
            float phi = t * deltaPhi;
            vec3 v = generateSample(normal, theta, phi);
            int index = s * numSamples + t;
            sampleVertexBuffer.vertices[index * 2] = vec4(0.f, 0.f, 0.f, 1.0f);
            sampleVertexBuffer.vertices[index * 2 + 1] = vec4(v, 1.f);
        }
    }
    // Draw a line for the normal
    sampleVertexBuffer.vertices[numSamples * numSamples * 2] = vec4(0.0f, 0.f, 0.f, 2.f);
    sampleVertexBuffer.vertices[numSamples * numSamples * 2 + 1] = vec4(normal, 2.f);
}

void main()
{
    vec3 n = normalize(fragmentObjPos);
    vec3 result = vec3(0.f);
    // need to be careful with number of samples because it could cause the gpu driver crashes with
    // "fata program exit requested" 
    float delta = 0.05f;
    float numSampleTheta = 0.5f * pi / delta;
    float numSamplePhi = 2.0f * pi / delta;
    for (int s = 0; s < numSampleTheta; s++)
    {
        float theta = s * delta;
        for (int t = 0; t < numSamplePhi; t++)
        {
            float phi = t * delta;
            vec3 r = generateSample(n, theta, phi);
            result += texture(srcCubemapTexture, r).rgb * cos(theta) * sin(theta);
        }
    }
    // the denominator, divide by N comes from solid angle in the integration formula
    result = pi * result / (numSampleTheta * numSamplePhi);
    // No need for tonemapping at the end as the result will be used in lighting instead of being
    // displayed by the framebuffer
    fragColor = vec4(result, 1.f);
}