#version 450 core

#define pi 3.14159265359

in VSOutput
{
    vec3 objectSpacePosition;
} psIn;

out vec3 outReflection;

uniform float roughness;
uniform samplerCube srcCubemap;

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

vec2 hammersley(uint i, float numSamples) 
{
    uint bits = i;
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555) << 1) | ((bits & 0xAAAAAAAA) >> 1);
    bits = ((bits & 0x33333333) << 2) | ((bits & 0xCCCCCCCC) >> 2);
    bits = ((bits & 0x0F0F0F0F) << 4) | ((bits & 0xF0F0F0F0) >> 4);
    bits = ((bits & 0x00FF00FF) << 8) | ((bits & 0xFF00FF00) >> 8);
    return vec2(i / numSamples, bits / exp2(32));
}

vec3 importanceSampleGGX(vec3 n, float roughness, vec2 xi)
{
	float a = roughness;

	float phi = 2 * pi * xi.x;
	float cosTheta = sqrt((1 - xi.y) / ( 1 + (a*a - 1) * xi.y));
	float sinTheta = sqrt(1 - cosTheta * cosTheta);

	vec3 h;
	h.x = sinTheta * cos( phi );
	h.y = sinTheta * sin( phi );
	h.z = cosTheta;

	vec3 up = abs(n.z) < 0.999 ? vec3(0, 0.f, 1.f) : vec3(1.f, 0, 0.f);
	vec3 tangentX = normalize( cross( up, n ) );
	vec3 tangentY = cross( n, tangentX);
	// tangent to world space
	return tangentX * h.x + tangentY * h.y + n * h.z;
}

void main()
{
    vec3 n = normalize(psIn.objectSpacePosition);
    // fix viewDir
    vec3 viewDir = n;

    float remappedRoughness = roughness * roughness;

    uint numSamples = 1024;
    outReflection = vec3(0.f);
    float weight = 0.f; 
    for (int i = 0; i < numSamples; i++)
    {
        vec2 xi = hammersley(i, numSamples);
        vec3 h = importanceSampleGGX(n, remappedRoughness, xi);
        vec3 l = -reflect(viewDir, h);
        float ndotl = saturate(dot(n, l));
        float ndoth = saturate(dot(n, h));
        float vdoth = saturate(dot(h, viewDir));
        if (ndotl > 0)
        {
            outReflection += texture(srcCubemap, l).rgb * ndotl;
            weight += ndotl;
        }
    }
    // according to Epic's notes, weighted by ndotl produce better visual results
    outReflection /= weight; 
}