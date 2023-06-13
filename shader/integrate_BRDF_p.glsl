#version 450 core

#define pi 3.14159265359

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec4 DFG;

float saturate(float v)
{
    return clamp(v, 0.f, 1.f);
}

/**
 * The 4 * notl * ndotv in the original G_SmithGGX is cancelled with thet
    4 * ndotl * ndotv term in the denominator of Cook-Torrance BRDF, forming this 
    simplified V term. Thus when using this V term, the BRDF becomes D * F * V without
    needing to divide by 4 * ndotl * ndotv.
 */
float V_SmithGGXHeightCorrelated(float ndotv, float ndotl, float roughness)
{
    float a2 = roughness * roughness;
    float v = ndotl * sqrt(ndotv * ndotv * (1.0 - a2) + a2);
    float l = ndotv * sqrt(ndotl * ndotl * (1.0 - a2) + a2);
    return 0.5f / (v + l);
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
    float ndotv = psIn.texCoord0.x; 
    float roughness = (psIn.texCoord0.y * psIn.texCoord0.y);
    float sinTheta = sqrt(1 - ndotv * ndotv);
    vec3 v = vec3(sinTheta, 0.f, ndotv);
    vec3 n = vec3(0.f, 0.f, 1.f);
    float A = 0.f;
    float B = 0.f;
    uint numSamples = 1024u;
    for (uint i = 0; i < numSamples; ++i)
    {
        vec2 xi = hammersley(i, float(numSamples));
        vec3 h = importanceSampleGGX(n, roughness, xi);
        vec3 l = normalize(-reflect(v, h));

        float ndotl = saturate(dot(n, l));
        float ndoth = saturate(dot(n, h));
        float vdoth = saturate(dot(v, h));

        if (ndotl > 0.f)
        {
            float V = V_SmithGGXHeightCorrelated(ndotv, ndotl, roughness);
            float G = 4.f * ndotl * V * vdoth / ndoth;

            float fc = pow(1.f - vdoth, 5.f);
            A += (1.f - fc) * G;
            B += fc * G;
        }
    }
    A /= numSamples;
    B /= numSamples;
    DFG = vec4(A, B, 0.f, 1.f);
}
