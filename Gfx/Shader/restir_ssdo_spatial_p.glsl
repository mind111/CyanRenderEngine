#version 450 core

#define PI 3.1415926

in VSOutput
{
	vec2 texCoord0;
} psIn;

layout (location = 0) out vec3 outReservoirRadiance;
layout (location = 1) out vec3 outReservoirDirection;
layout (location = 2) out vec3 outReservoirWSumMW;

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

uniform sampler2D u_reservoirSkyRadiance;
uniform sampler2D u_reservoirSkyDirection;
uniform sampler2D u_reservoirSkyWSumMW;

struct ReservoirSample
{
	vec3 radiance;
	vec3 direction;
};

struct Reservoir
{
	ReservoirSample z;
    float wSum;
    float M;
    float W;
};

Reservoir sampleReservoir(in vec2 sampleCoord)
{
    Reservoir r;
    r.z.radiance = texture(u_reservoirSkyRadiance, sampleCoord).rgb;
    r.z.direction = texture(u_reservoirSkyDirection, sampleCoord).rgb;
    vec3 wSumMW = texture(u_reservoirSkyWSumMW, sampleCoord).rgb;
    r.wSum = wSumMW.x;
    r.M = wSumMW.y;
    r.W = wSumMW.z;
    return r;
}

float calcLuminance(vec3 inLinearColor)
{
    return 0.2126 * inLinearColor.r + 0.7152 * inLinearColor.g + 0.0722 * inLinearColor.b;
}

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

void mergeReservoir(inout Reservoir r, in Reservoir rr, float jacobian, vec3 n)
{
	float pq = calcLuminance(rr.z.radiance) * max(dot(n, rr.z.direction), 0.f);
	float wi = pq * rr.W * rr.M * jacobian;

    r.wSum += wi;
    float p = wi / r.wSum;
    if (get_random().x < p)
    {
		r.z = rr.z;
    }
	r.M += rr.M;

	float targetPdf = calcLuminance(r.z.radiance) * max(dot(n, r.z.direction), 0.f);
	r.W = r.wSum / max(r.M * targetPdf, 1e-4);
}

bool isValidScreenCoord(vec2 coord)
{
	return (coord.x >= 0.f && coord.x <= 1.f && coord.y >= 0.f && coord.y <= 1.f); 
}

vec3 screenToWorld(in vec2 pixelCoord, in sampler2D depthTex, mat4 viewMatrix, mat4 projectionMatrix)
{
	float depth = texture(depthTex, pixelCoord).r;
	vec3 NDCSpacePos = vec3(pixelCoord, depth) * 2.f - 1.f;
    vec4 clipSpacePos = inverse(projectionMatrix) * vec4(NDCSpacePos, 1.f);
    vec3 viewSpacePos = clipSpacePos.xyz / clipSpacePos.w;
    vec3 worldSpacePos = (inverse(viewMatrix) * vec4(viewSpacePos, 1.f)).xyz;
    return worldSpacePos;
}

vec3 screenToView(in vec2 pixelCoord, in sampler2D depthTex, mat4 projectionMatrix)
{
	float depth = texture(depthTex, pixelCoord).r;
	vec3 NDCSpacePos = vec3(pixelCoord, depth) * 2.f - 1.f;
    vec4 clipSpacePos = inverse(projectionMatrix) * vec4(NDCSpacePos, 1.f);
    vec3 viewSpacePos = clipSpacePos.xyz / clipSpacePos.w;
    return viewSpacePos;
}

struct ViewParameters
{
	uvec2 renderResolution;
	float aspectRatio;
	mat4 viewMatrix;
    mat4 prevFrameViewMatrix;
	mat4 projectionMatrix;
    mat4 prevFrameProjectionMatrix;
	vec3 cameraPosition;
	vec3 prevFrameCameraPosition;
	vec3 cameraRight;
	vec3 cameraForward;
	vec3 cameraUp;
	int frameCount;
	float elapsedTime;
	float deltaTime;
};

uniform ViewParameters viewParameters;
uniform sampler2D u_sceneDepthTex;
uniform sampler2D u_sceneNormalTex;
uniform float u_kernelRadius;
uniform int u_sampleCount;
uniform int u_passIndex;

void main()
{
    seed = viewParameters.frameCount;
    flat_idx = int(floor(gl_FragCoord.y) * viewParameters.renderResolution.x + floor(gl_FragCoord.x));
    vec2 rng2 = get_random().xy;

    float angle = rng2.x * PI * 2.f;
    mat2 perPixelRandomRotation = {
		{ cos(angle), sin(angle) },
		{ -sin(angle), cos(angle) }
    };

	float deviceDepth = texture(u_sceneDepthTex, psIn.texCoord0).r; 
	vec3 n = texture(u_sceneNormalTex, psIn.texCoord0).rgb * 2.f - 1.f;

    if (deviceDepth > 0.999999f) discard;

    mat4 view = viewParameters.viewMatrix, projection = viewParameters.projectionMatrix;
    vec3 worldSpacePosition = screenToWorld(psIn.texCoord0, u_sceneDepthTex, view, projection);
    vec3 viewSpacePosition = screenToView(psIn.texCoord0, u_sceneDepthTex, projection);
    float linearDepth = abs(viewSpacePosition.z);

    vec2 centerTapCoord = psIn.texCoord0;
    Reservoir r = sampleReservoir(centerTapCoord);
    for (int i = 0; i < u_sampleCount; ++i)
    {
        int blueNoiseSampleIndex = u_passIndex * u_sampleCount + i;
        vec2 neighborCoord = centerTapCoord + perPixelRandomRotation * BlueNoiseInDisk[blueNoiseSampleIndex] * u_kernelRadius; 
        if (!isValidScreenCoord(neighborCoord))
        {
            continue;
        }
        float neighborDepth = texture(u_sceneDepthTex, neighborCoord).r;
        if (neighborDepth > 0.999999f)
        {
			continue;
        }

        Reservoir rn = sampleReservoir(neighborCoord);

        vec3 neighborWorldSpacePos = screenToWorld(neighborCoord, u_sceneDepthTex, view, projection);
        vec3 neighborViewSpacePos = screenToView(neighborCoord, u_sceneDepthTex, projection);
        float neighborLinearDepth = abs(neighborViewSpacePos.z);
        vec3 neighborNormal = texture(u_sceneNormalTex, neighborCoord).rgb * 2.f - 1.f;

        // reducing bias by rejecting neighboring samples that have too much geometric difference
        float depthDiff = abs(neighborLinearDepth - linearDepth) / linearDepth;
        float normalDiff = dot(n, neighborNormal);
        // todo: visualize area being rejected due to geometric difference
        // todo: instead of using a hard cutoff, maybe attenuate the reservoir weight instead
        if (normalDiff < 0.8f || depthDiff > 0.1f) 
        {
			continue;
		}

        // todo: calc reuse jacobian
		mergeReservoir(r, rn, 1.f, n);
    }

    // clamp spatial reservoir M to something that makes sense or else firefly would appear
    const int kMaxSpatialReservoirM = 8;
	if (r.M >= kMaxSpatialReservoirM)
	{
		int clampedSampleCount = kMaxSpatialReservoirM - 1;
		float averageW = r.wSum / r.M;
		// reduce wSum
		r.wSum -= (r.M - clampedSampleCount) * averageW;
		r.wSum = max(0.f, r.wSum);
		// clamp M 
		r.M = clampedSampleCount;
        // update W
		float targetPdf = calcLuminance(r.z.radiance) * max(dot(n, r.z.direction), 0.f);
		r.W = r.wSum / max(r.M * targetPdf, 1e-1);
	}

	outReservoirRadiance = r.z.radiance;
	outReservoirDirection = r.z.direction;
	outReservoirWSumMW = vec3(r.wSum, r.M, r.W);
}
