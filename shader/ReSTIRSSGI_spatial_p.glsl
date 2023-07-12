#version 450 core

#define PI 3.1415926

in VSOutput
{
	vec2 texCoord0;
} psIn;

layout (location = 0) out vec3 outReservoirRadiance;
layout (location = 1) out vec3 outReservoirPosition;
layout (location = 2) out vec3 outReservoirNormal;
layout (location = 3) out vec3 outReservoirWSumMW;

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
uniform sampler2D sceneDepthBuffer;
uniform sampler2D sceneNormalBuffer;

uniform int reusePass;
uniform int reuseSampleCount;
uniform float reuseRadius;

uniform sampler2D inReservoirRadiance;
uniform sampler2D inReservoirPosition;
uniform sampler2D inReservoirNormal;
uniform sampler2D inReservoirWSumMW;
uniform float bUseJacobian;

struct ReservoirSample
{
	vec3 radiance;
	vec3 position;
	vec3 normal;
};

struct Reservoir
{
	ReservoirSample z;
    float wSum;
    float M;
    float W;
};

float calcLuminance(vec3 inLinearColor)
{
    return 0.2126 * inLinearColor.r + 0.7152 * inLinearColor.g + 0.0722 * inLinearColor.b;
}

/**
 * this assumes that 
 */
float calcReSTIRGISampleWeight(in ReservoirSample s)
{
	float srcPdf = 1.f / (2.f * PI);
	float targetPdf = calcLuminance(s.radiance);
	return targetPdf / srcPdf;
}

Reservoir createEmptyReservoir()
{
	Reservoir r;
	r.z.radiance = vec3(0.f);
	r.z.position = vec3(0.f);
	r.z.normal = vec3(0.f);
	r.wSum = 0.f;
	r.M = 0.f;
	r.W = 0.f;
	return r;
}

Reservoir createFreshReservoirWithSample(in ReservoirSample s, float weight)
{
	Reservoir r;
	r.z = s;
	r.wSum = weight;
	r.M = 1.f;
	float targetPdf = calcLuminance(r.z.radiance);
	// r.W = targetPdf > 1e-4 ? r.wSum / (r.M * targetPdf) : 0.f;
	r.W = r.wSum / max(r.M * targetPdf, 1e-4);
	return r;
}

Reservoir sampleReservoir(in vec2 sampleCoord)
{
    Reservoir r;
    r.z.radiance = texture(inReservoirRadiance, sampleCoord).rgb;
    r.z.position = texture(inReservoirPosition, sampleCoord).rgb;
    r.z.normal = texture(inReservoirNormal, sampleCoord).rgb * 2.f - 1.f;
    vec3 wSumMW = texture(inReservoirWSumMW, sampleCoord).rgb;
    r.wSum = wSumMW.x;
    r.M = wSumMW.y;
    r.W = wSumMW.z;
    return r;
}

void updateReservoir(inout Reservoir r, in ReservoirSample s, in float weight)
{
    r.wSum += weight;
    float probablity = weight / r.wSum;
    if (get_random().x < probablity)
    {
		r.z = s;
    }

    r.M += 1.f;

	float targetPdf = calcLuminance(r.z.radiance);
	// r.W = targetPdf > 1e-4 ? r.wSum / (r.M * targetPdf) : 0.f;
	r.W = r.wSum / max(r.M * targetPdf, 1e-4);
}

void mergeReservoir(inout Reservoir r, in Reservoir rr, float jacobian)
{
	float pq = calcLuminance(rr.z.radiance);
	float wi = pq * rr.W * rr.M * jacobian;

    r.wSum += wi;
    float p = wi / r.wSum;
    if (get_random().x < p)
    {
		r.z = rr.z;
    }
	r.M += rr.M;

	float targetPdf = calcLuminance(r.z.radiance);
	// r.W = targetPdf > 1e-4 ? r.wSum / (r.M * targetPdf) : 0.f;
	r.W = r.wSum / max(r.M * targetPdf, 1e-4);
}

vec3 worldToScreen(vec3 pp, in mat4 view, in mat4 projection)
{
    vec4 p = projection * view * vec4(pp, 1.f);
    p /= p.w;
    return p.xyz * .5f + .5f;
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

void main()
{
    seed = 0;
    flat_idx = int(floor(gl_FragCoord.y) * viewParameters.renderResolution.x + floor(gl_FragCoord.x));
    vec2 rng2 = get_random().xy;

    float angle = rng2.x * PI * 2.f;
    mat2 perPixelRandomRotation = {
		{ cos(angle), sin(angle) },
		{ -sin(angle), cos(angle) }
    };

	float deviceDepth = texture(sceneDepthBuffer, psIn.texCoord0).r; 
	vec3 n = texture(sceneNormalBuffer, psIn.texCoord0).rgb * 2.f - 1.f;

    if (deviceDepth > 0.999999f) discard;

    mat4 view = viewParameters.viewMatrix, projection = viewParameters.projectionMatrix;
    vec3 worldSpacePosition = screenToWorld(psIn.texCoord0, sceneDepthBuffer, view, projection);
    vec3 viewSpacePosition = screenToView(psIn.texCoord0, sceneDepthBuffer, projection);
    float linearDepth = abs(viewSpacePosition.z);

    vec2 centerTapCoord = psIn.texCoord0;
    Reservoir r = sampleReservoir(centerTapCoord);
    for (int i = 0; i < reuseSampleCount; ++i)
    {
        int blueNoiseSampleIndex = reusePass * reuseSampleCount + i;
        vec2 neighborCoord = centerTapCoord + perPixelRandomRotation * BlueNoiseInDisk[blueNoiseSampleIndex] * (reuseRadius / pow(2.f, i)); 
        if (!isValidScreenCoord(neighborCoord))
        {
            continue;
        }

        Reservoir rn = sampleReservoir(neighborCoord);

        vec3 neighborWorldSpacePos = screenToWorld(neighborCoord, sceneDepthBuffer, view, projection);
        vec3 neighborViewSpacePos = screenToView(neighborCoord, sceneDepthBuffer, projection);
        float neighborLinearDepth = abs(neighborViewSpacePos.z);
        vec3 neighborNormal = texture(sceneNormalBuffer, neighborCoord).rgb * 2.f - 1.f;

        // reducing bias by rejecting neighboring samples that have too much geometric difference
        float depthDiff = abs(neighborLinearDepth - linearDepth) / linearDepth;
        float normalDiff = dot(n, neighborNormal);
        // todo: visualize area being rejected due to geometric difference
        // todo: instead of using a hard cutoff, maybe attenuate the reservoir weight instead
        if (normalDiff < 0.8f || depthDiff > 0.1f) 
        {
			continue;
		}

		// todo: test for the neighbor reservoir is visible

        // calc reuse jacobian
		vec3 x1r = worldSpacePosition;
		vec3 x1q = neighborWorldSpacePos;
		vec3 x2q = rn.z.position;
		vec3 x1qx2q = x1q - x2q;
		vec3 x1rx2q = x1r - x2q;
		float a = dot(normalize(x1rx2q), rn.z.normal) / max(dot(normalize(x1qx2q), rn.z.normal), 1e-1);
		float b = dot(x1qx2q, x1qx2q) / max(dot(x1rx2q, x1rx2q), 1e-4);
		float jacobian = a * b;

		mergeReservoir(r, rn, jacobian);
    }

	outReservoirRadiance = r.z.radiance;
	outReservoirPosition = r.z.position;
	outReservoirNormal = r.z.normal * .5f + .5f;
	outReservoirWSumMW = vec3(r.wSum, r.M, r.W);
}
