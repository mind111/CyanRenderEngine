#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

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

/* 
    non-height correlated smith G2
*/
/*
float ggxSmithG1(vec3 v, vec3 h, vec3 n, float roughness)
{
    float ndotv = dot(n, v); 
    float hdotv = dot(h, v); 
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float tangentTheta2 = (1 - ndotv * ndotv) / max(0.001f, (ndotv * ndotv));
    return 2.f / (1.f + sqrt(1.f + alpha2 * tangentTheta2));
}

float ggxSmithG2Ex(vec3 v, vec3 l, vec3 n, float roughness)
{
    vec3 h = normalize(v + n);
    return ggxSmithG1(v, h, n, roughness) * ggxSmithG1(l, h, n, roughness);
}
*/

float ggxSmithG1(vec3 v, vec3 h, vec3 n, float roughness)
{
    float a = roughness * roughness;
    float k = (a * a) / 2.0;
    float nom   = saturate(dot(n, v));
    float denom = saturate(dot(n, v)) * (1.0 - k) + k;
    return nom / max(denom, 0.1);
}

// ----------------------------------------------------------------------------
float ggxSmithG(vec3 n, vec3 v, vec3 l, float roughness)
{
    vec3 h = normalize(v + l);
    float ggx1 = ggxSmithG1(l, h, n, roughness);
    float ggx2 = ggxSmithG1(v, h, n, roughness);
    return ggx1 * ggx2;
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
    float ndotv = psIn.texCoord0.x; 
    float roughness = psIn.texCoord0.y;
    float sinTheta = sqrt(1 - ndotv * ndotv);
    vec3 v = vec3(sinTheta, ndotv, 0.f);
    uint numSamples = 1024u;
    vec3 n = vec3(0.f, 1.f, 0.f);
    float A = 0.f;
    float B = 0.f;
    for (uint s = 0; s < numSamples; ++s)
    {
        vec2 rand_uv = HammersleyNoBitOps(s, numSamples);
        vec3 h = importanceSampleGGX(n, roughness, rand_uv.x, rand_uv.y);
        vec3 l = normalize(-reflect(v, h));

        float ndotl = saturate(dot(n, l));
        float ndoth = saturate(dot(n, h));
        float vdoth = saturate(dot(v, h));

        if (ndotl > 0.f)
        {
            float G = ggxSmithG(n, v, l, roughness);
            float GVis = G * vdoth / (ndotv * ndoth);
            float a = (1.f - vdoth) * (1.f - vdoth) * (1.f - vdoth) * (1.f - vdoth) * (1.f - vdoth);
            A += (1.f - a) * GVis;
            B += a         * GVis;
        }
    }
    A /= numSamples;
    B /= numSamples;
    fragColor = vec4(A, B, 0.f, 1.f);
}
