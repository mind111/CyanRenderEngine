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

/*
vec3 importanceSampleGGX(vec3 n, float roughness, float rand_u, float rand_v)
{
    float theta = atan(roughness * sqrt(rand_u) / max(sqrt(1 - rand_u), 0.01));
    float phi = 2 * pi * rand_v;
    return generateSample(n, theta, phi);
}
*/

vec3 importanceSampleGGX(vec3 n, float roughness, float rand_u, float rand_v)
{
	float a = roughness * roughness;
	float phi = 2 * pi * rand_u;
	float cosTheta = sqrt( (1 - rand_v) / ( 1 + (a*a - 1) * rand_v ) );
	float sinTheta = sqrt( 1 - cosTheta * cosTheta );
	vec3 h;
	h.x = sinTheta * sin( phi );
	h.y = sinTheta * cos( phi );
	h.z = cosTheta;
	vec3 up = abs(n.y) < 0.999 ? vec3(0, 1.f, 0.f) : vec3(0.f, 0, 1.f);
	vec3 tangentX = normalize( cross( n, up ) );
	vec3 tangentY = cross( n, tangentX);
	// tangent to world space
	return tangentX * h.x + tangentY * h.y + n * h.z;
}

float GGX(float roughness, float ndoth) 
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float result = alpha2;
    float denom = ndoth * ndoth * (alpha2 - 1.f) + 1.f;
    result /= max((pi * denom * denom), 0.001); 
    return result;
}

void main()
{
    vec3 n = normalize(psIn.objectSpacePosition);
    // fix viewDir
    vec3 viewDir = n;
    uint numSamples = 1024;
    outReflection = vec3(0.f);
    float weight = 0.f; 
    for (int s = 0; s < numSamples; s++)
    {
        vec2 uv = HammersleyNoBitOps(s, numSamples);
        vec3 h = importanceSampleGGX(n, roughness, uv.x, uv.y);
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