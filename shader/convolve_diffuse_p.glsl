#version 450 core

#define pi 3.14159265359

in VSOutput
{
    vec3 objectSpacePosition;
} psIn;

out vec3 outIrradiance;


float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

vec3 calcSampleDir(vec3 n, float theta, float phi)
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

// Watchout for padding if using vec3
layout(std430, binding = 3) buffer sampleVertexData 
{
    vec4 vertices[];
} sampleVertexBuffer;

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
            vec3 v = calcSampleDir(normal, theta, phi);
            int index = s * numSamples + t;
            sampleVertexBuffer.vertices[index * 2] = vec4(0.f, 0.f, 0.f, 1.0f);
            sampleVertexBuffer.vertices[index * 2 + 1] = vec4(v, 1.f);
        }
    }
    // Draw a line for the normal
    sampleVertexBuffer.vertices[numSamples * numSamples * 2] = vec4(0.0f, 0.f, 0.f, 2.f);
    sampleVertexBuffer.vertices[numSamples * numSamples * 2 + 1] = vec4(normal, 2.f);
}

uniform samplerCube srcCubemap;
uniform float numSamplesInTheta;
uniform float numSamplesInPhi;

void main()
{
    outIrradiance = vec3(0.f);

    vec3 n = normalize(psIn.objectSpacePosition);
    float N = numSamplesInTheta * numSamplesInPhi;
    float deltaTheta = .5f * pi / numSamplesInTheta;
    float deltaPhi = 2.f * pi / numSamplesInPhi;
    for (int s = 0; s < int(numSamplesInTheta); s++)
    {
        float theta = s * deltaTheta;
        for (int t = 0; t < int(numSamplesInPhi); t++)
        {
            float phi = t * deltaPhi;
            vec3 sampleDir = calcSampleDir(n, theta, phi);
            outIrradiance += texture(srcCubemap, sampleDir).rgb * cos(theta) * sin(theta);
        }
    }
    /*
    * Referencing this article http://www.codinglabs.net/article_physically_based_rendering.aspx
      for the pi term, though I don't understand its derivation. Not including it does make the output 
      irradiance feel too dark, so including it for now.
    */
    outIrradiance = pi * outIrradiance / N;
}