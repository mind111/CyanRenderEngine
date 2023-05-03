#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

struct Reservoir
{
    vec3 sampleRadiance;
    vec3 samplePosition;
    vec3 sampleNormal;
    float wSum;
    float M;
    float W;
};

layout(std430) buffer ViewBuffer
{
    mat4  view;
    mat4  projection;
};

layout (location = 0) out vec3 outReservoirRadiance;
layout (location = 1) out vec3 outReservoirSamplePosition;
layout (location = 2) out vec3 outReservoirSampleNormal;
layout (location = 3) out vec3 outReservoirWSumMW;
layout (location = 4) out vec3 outIndirectIrradiance;

/**
* blue noise samples on a unit disk taken from https://www.shadertoy.com/view/3sfBWs
*/
const vec2 BlueNoiseInDisk[64] = vec2[64](
    vec2(0.478712,0.875764),
    vec2(-0.337956,-0.793959),
    vec2(-0.955259,-0.028164),
    vec2(0.864527,0.325689),
    vec2(0.209342,-0.395657),
    vec2(-0.106779,0.672585),
    vec2(0.156213,0.235113),
    vec2(-0.413644,-0.082856),
    vec2(-0.415667,0.323909),
    vec2(0.141896,-0.939980),
    vec2(0.954932,-0.182516),
    vec2(-0.766184,0.410799),
    vec2(-0.434912,-0.458845),
    vec2(0.415242,-0.078724),
    vec2(0.728335,-0.491777),
    vec2(-0.058086,-0.066401),
    vec2(0.202990,0.686837),
    vec2(-0.808362,-0.556402),
    vec2(0.507386,-0.640839),
    vec2(-0.723494,-0.229240),
    vec2(0.489740,0.317826),
    vec2(-0.622663,0.765301),
    vec2(-0.010640,0.929347),
    vec2(0.663146,0.647618),
    vec2(-0.096674,-0.413835),
    vec2(0.525945,-0.321063),
    vec2(-0.122533,0.366019),
    vec2(0.195235,-0.687983),
    vec2(-0.563203,0.098748),
    vec2(0.418563,0.561335),
    vec2(-0.378595,0.800367),
    vec2(0.826922,0.001024),
    vec2(-0.085372,-0.766651),
    vec2(-0.921920,0.183673),
    vec2(-0.590008,-0.721799),
    vec2(0.167751,-0.164393),
    vec2(0.032961,-0.562530),
    vec2(0.632900,-0.107059),
    vec2(-0.464080,0.569669),
    vec2(-0.173676,-0.958758),
    vec2(-0.242648,-0.234303),
    vec2(-0.275362,0.157163),
    vec2(0.382295,-0.795131),
    vec2(0.562955,0.115562),
    vec2(0.190586,0.470121),
    vec2(0.770764,-0.297576),
    vec2(0.237281,0.931050),
    vec2(-0.666642,-0.455871),
    vec2(-0.905649,-0.298379),
    vec2(0.339520,0.157829),
    vec2(0.701438,-0.704100),
    vec2(-0.062758,0.160346),
    vec2(-0.220674,0.957141),
    vec2(0.642692,0.432706),
    vec2(-0.773390,-0.015272),
    vec2(-0.671467,0.246880),
    vec2(0.158051,0.062859),
    vec2(0.806009,0.527232),
    vec2(-0.057620,-0.247071),
    vec2(0.333436,-0.516710),
    vec2(-0.550658,-0.315773),
    vec2(-0.652078,0.589846),
    vec2(0.008818,0.530556),
    vec2(-0.210004,0.519896) 
);

/* note: 
* rand number generator taken from https://www.shadertoy.com/view/4lfcDr
*/
uint flat_idx;
uint seed;
void encrypt_tea(inout uvec2 arg)
{
	uvec4 key = uvec4(0xa341316c, 0xc8013ea4, 0xad90777d, 0x7e95761e);
	uint v0 = arg[0], v1 = arg[1];
	uint sum = 0u;
	uint delta = 0x9e3779b9u;

	for(int i = 0; i < 32; i++) {
		sum += delta;
		v0 += ((v1 << 4) + key[0]) ^ (v1 + sum) ^ ((v1 >> 5) + key[1]);
		v1 += ((v0 << 4) + key[2]) ^ (v0 + sum) ^ ((v0 >> 5) + key[3]);
	}
	arg[0] = v0;
	arg[1] = v1;
}

vec2 get_random()
{
  	uvec2 arg = uvec2(flat_idx, seed++);
  	encrypt_tea(arg);
  	return fract(vec2(arg) / vec2(0xffffffffu));
}

uniform sampler2D reservoirRadiance;
uniform sampler2D reservoirSamplePosition;
uniform sampler2D reservoirSampleNormal;
uniform sampler2D reservoirWSumMW;
uniform float reusePass;

Reservoir getReservoir(vec2 sampleCoord)
{
    Reservoir r;
    r.sampleRadiance = texture(reservoirRadiance, sampleCoord).rgb;
    r.samplePosition = texture(reservoirSamplePosition, sampleCoord).xyz;
    r.sampleNormal = normalize(texture(reservoirSampleNormal, sampleCoord).xyz * 2.f - 1.f);
    vec3 wSumMW = texture(reservoirWSumMW, sampleCoord).xyz;
    r.wSum = wSumMW.x;
    r.M = wSumMW.y;
    r.W = wSumMW.z;
    return r;
}

float calcLuminance(vec3 inLinearColor)
{
    return 0.2126 * inLinearColor.r + 0.7152 * inLinearColor.g + 0.0722 * inLinearColor.b;
}

void mergeReservoir(inout Reservoir merged, in Reservoir r, in float wi)
{
    merged.wSum += wi;
    float p = wi / merged.wSum;
    if (get_random().x < p)
    {
        merged.sampleRadiance = r.sampleRadiance;
        merged.samplePosition = r.samplePosition;
        merged.sampleNormal = r.sampleNormal;
    }
    merged.M += r.M;
    float targetPdf = calcLuminance(merged.sampleRadiance);
    merged.W = merged.wSum / (targetPdf * merged.M);
}

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) 
{
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
}

vec3 worldToScreen(vec3 pp, in mat4 view, in mat4 projection)
{
    vec4 p = projection * view * vec4(pp, 1.f);
    p /= p.w;
    return p.xyz * .5f + .5f;
}

uniform sampler2D sceneDepthBuffer;
uniform sampler2D sceneNormalBuffer;
uniform vec2 outputSize; 
uniform int numSamples;

void main()
{
    seed = 0;
    flat_idx = int(floor(gl_FragCoord.y) * outputSize.x + floor(gl_FragCoord.x));

	float deviceDepth = texture(sceneDepthBuffer, psIn.texCoord0).r; 
	vec3 n = texture(sceneNormalBuffer, psIn.texCoord0).rgb * 2.f - 1.f;

    if (deviceDepth > 0.998f) discard;

    vec3 worldSpacePosition = screenToWorld(vec3(psIn.texCoord0, deviceDepth) * 2.f - 1.f, inverse(view), inverse(projection));
    
    Reservoir r = getReservoir(psIn.texCoord0);

	// todo: random rotation 
    vec2 rand = get_random().xy;
    float randomRotAngle = rand.x;
    float randomRadius = rand.y;
	mat2 rotation = {
		{ cos(randomRotAngle), sin(randomRotAngle) },
		{ -sin(randomRotAngle), cos(randomRotAngle) }
	};
    for (int i = 0; i < numSamples; ++i)
    {
        vec2 sampleCoord = psIn.texCoord0 + rotation * BlueNoiseInDisk[i] * 0.02f * randomRadius; 

        if (sampleCoord.x < 0.f || sampleCoord.x > 1.f || sampleCoord.y < 0.f || sampleCoord.y > 1.f)
        {
			continue;
        }

        Reservoir rr = getReservoir(sampleCoord);

        float neighborDeviceZ = texture(sceneDepthBuffer, sampleCoord).r;
        vec3 neighborWorldSpacePos = screenToWorld(vec3(sampleCoord, neighborDeviceZ) * 2.f - 1.f, inverse(view), inverse(projection));
        vec3 neighborNormal = texture(sceneNormalBuffer, sampleCoord).rgb * 2.f - 1.f;
        // todo: reject samples based on depth and normal difference
        // todo: is it necessary to reject neighboring reservoir's sample if it's not in the upper hemisphere

        vec3 x1q = neighborWorldSpacePos;
        vec3 x2q = rr.samplePosition;
        vec3 x1r = worldSpacePosition;
        vec3 x1qx2q = x1q - x2q;
        vec3 x1rx2q = x1r - x2q;

        float a = abs(dot(normalize(x1rx2q), rr.sampleNormal)) / abs(dot(normalize(x1qx2q), rr.sampleNormal));
        float b = dot(x1qx2q, x1qx2q) / dot(x1rx2q, x1rx2q);
        float jacobian = a * b;
        mergeReservoir(r, rr, rr.wSum);
    }

    if (reusePass > 0.f)
    {
		outReservoirRadiance = r.sampleRadiance;
		outReservoirSamplePosition = r.samplePosition;
		outReservoirSampleNormal = r.sampleNormal;
		outReservoirWSumMW = vec3(r.wSum, r.M, r.W);
	}
    else
    {
		float ndotl = max(dot(normalize(r.samplePosition - worldSpacePosition), n), 0.f);
		outIndirectIrradiance = r.sampleRadiance * ndotl * r.W;
    }
}
