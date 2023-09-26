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

uniform float u_numSamplesInTheta;
uniform float u_numSamplesInPhi;
uniform samplerCube u_srcCubemap;

void main()
{
    outIrradiance = vec3(0.f);

    vec3 n = normalize(psIn.objectSpacePosition);
    float N = u_numSamplesInTheta * u_numSamplesInPhi;
    float deltaTheta = .5f * pi / u_numSamplesInTheta;
    float deltaPhi = 2.f * pi / u_numSamplesInPhi;
    for (int s = 0; s < int(u_numSamplesInTheta); s++)
    {
        float theta = s * deltaTheta;
        for (int t = 0; t < int(u_numSamplesInPhi); t++)
        {
            float phi = t * deltaPhi;
            vec3 sampleDir = calcSampleDir(n, theta, phi);
            outIrradiance += texture(u_srcCubemap, sampleDir).rgb * cos(theta) * sin(theta);
        }
    }
    /*
    * Referencing this article http://www.codinglabs.net/article_physically_based_rendering.aspx
      for the pi term, though I don't understand its derivation. Not including it does make the output 
      irradiance feel too dark, so including it for now.
    */
    outIrradiance = pi * outIrradiance / N;
}
