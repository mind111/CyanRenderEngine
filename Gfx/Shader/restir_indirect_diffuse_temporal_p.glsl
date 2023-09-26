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
	// float srcPdf = 1.f / (2.f * PI);
	float srcPdf = 1.f;
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
	r.W = r.wSum / max(r.M * targetPdf, 1e-4);
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

// todo: write a shader #include thing
// #import "random.csh"

uniform sampler2D u_sceneDepthTex;
uniform sampler2D u_sceneNormalTex;
uniform sampler2D u_diffuseRadianceTex;
uniform sampler2D u_screenSpaceHitPositionTex;

uniform sampler2D u_prevFrameReservoirRadiance;
uniform sampler2D u_prevFrameReservoirPosition;
uniform sampler2D u_prevFrameReservoirNormal;
uniform sampler2D u_prevFrameReservoirWSumMW;
uniform sampler2D u_prevFrameSceneDepthTex;

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

vec3 reproject(in vec2 pixelCoord, in sampler2D depthTex, in sampler2D prevFrameDepthTex, in ViewParameters viewParameters)
{
	vec3 reprojected;
	float depth = texture(depthTex, pixelCoord).r;
	vec3 worldSpacePos = screenToWorld(pixelCoord, depthTex, viewParameters.viewMatrix, viewParameters.projectionMatrix);

	vec4 prevFrameClipPos = viewParameters.prevFrameProjectionMatrix * viewParameters.prevFrameViewMatrix * vec4(worldSpacePos, 1.f);
	vec3 prevFrameNDC = prevFrameClipPos.xyz / prevFrameClipPos.w;
	reprojected = prevFrameNDC * .5f + .5f;
	return reprojected;
}

// reproject
// todo: take pixel velocity into consideration when doing reprojection
// todo: consider changes in pixel neighborhood when reusing cache sample, changes in neighborhood pixels means SSAO value can change even there is a cache hit
// todo: adaptive convergence-aware spatial filtering
bool lookupTemporalReservoir(inout Reservoir r, in vec2 sampleCoord, sampler2D depthTex, sampler2D prevFrameDepthTex, in ViewParameters viewParameters)
{
	bool lookupResult = false;

	vec3 worldSpacePos = screenToWorld(sampleCoord, depthTex, viewParameters.viewMatrix, viewParameters.projectionMatrix);
	vec3 prevFrameViewSpacePos = (viewParameters.prevFrameViewMatrix * vec4(worldSpacePos, 1.f)).xyz;

	vec3 reprojectedCoord = reproject(sampleCoord, depthTex, prevFrameDepthTex, viewParameters);
	if (isValidScreenCoord(reprojectedCoord.xy))
	{
		vec3 cachedPrevFrameViewSpacePos = screenToView(reprojectedCoord.xy, prevFrameDepthTex, viewParameters.prevFrameProjectionMatrix);

		// compare relative depth
		float relativeDepthDiff = abs((prevFrameViewSpacePos.z - cachedPrevFrameViewSpacePos.z) / cachedPrevFrameViewSpacePos.z);
		if (relativeDepthDiff < .01f)
		{
			lookupResult = true;
			r.z.radiance = texture(u_prevFrameReservoirRadiance, reprojectedCoord.xy).rgb;
			r.z.position = texture(u_prevFrameReservoirPosition, reprojectedCoord.xy).xyz;
			r.z.normal = texture(u_prevFrameReservoirNormal, reprojectedCoord.xy).xyz * 2.f - 1.f;
			vec3 wSumMW = texture(u_prevFrameReservoirWSumMW, reprojectedCoord.xy).xyz;
			r.wSum = wSumMW.x; 
			r.M = wSumMW.y; 
			r.W = wSumMW.z;
		}
	}

	return lookupResult;
}


#define MAX_TEMPORAL_SAMPLE_COUNT 32
#define SKY_DIST 1e5

void main()
{
    seed = viewParameters.frameCount % 16;
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
	vec3 ro = screenToWorld(vec3(psIn.texCoord0, deviceDepth) * 2.f - 1.f, inverse(view), inverse(projection));
	vec3 worldSpacePosition = ro;

	vec3 screenSpaceHitPosition = texture(u_screenSpaceHitPositionTex, psIn.texCoord0).rgb;
	vec3 rd, hitPosition, hitNormal;
	vec3 incidentRadiance = vec3(0.f);
	if (screenSpaceHitPosition.z >= 2.f)
	{
		vec2 uv = screenSpaceHitPosition.xy;
		hitPosition = screenToWorld(vec3(uv, deviceDepth) * 2.f - 1.f, inverse(view), inverse(projection));
		rd = normalize(hitPosition - ro);
		hitNormal = texture(u_sceneNormalTex, uv).xyz * 2.f - 1.f;
		incidentRadiance = texture(u_diffuseRadianceTex, uv).rgb;
	}
	else
	{
		rd = screenSpaceHitPosition;
		hitPosition = ro + SKY_DIST * rd;
		hitNormal = -rd;
	}

	float ndotl = max(dot(n, rd), 0.f);
	ReservoirSample s;
	s.radiance = incidentRadiance;
	s.position = hitPosition;
	s.normal = hitNormal;
    Reservoir r = createFreshReservoirWithSample(s, calcReSTIRGISampleWeight(s));

    if (viewParameters.frameCount > 0)
    {
		Reservoir rHistoryCenter = createEmptyReservoir();

        // load temporal reservoir
        if (lookupTemporalReservoir(rHistoryCenter, psIn.texCoord0, u_sceneDepthTex, u_prevFrameSceneDepthTex, viewParameters))
		{
			// clamp history reservoir's M
			if (rHistoryCenter.M >= MAX_TEMPORAL_SAMPLE_COUNT)
			{
				int clampedSampleCount = MAX_TEMPORAL_SAMPLE_COUNT - 1;
				float averageW = rHistoryCenter.wSum / rHistoryCenter.M;
				// reduce wSum
				rHistoryCenter.wSum -= (rHistoryCenter.M - clampedSampleCount) * averageW;
				rHistoryCenter.wSum = max(0.f, rHistoryCenter.wSum);
				// clamp M 
				rHistoryCenter.M = clampedSampleCount;
			}

			// update history with new sample generated in current frame
			updateReservoir(rHistoryCenter, r.z, calcReSTIRGISampleWeight(r.z));
			r = rHistoryCenter;
		}
    }

    outReservoirRadiance = r.z.radiance;
    outReservoirPosition = r.z.position;
    outReservoirNormal = r.z.normal * .5f + .5f;
    outReservoirWSumMW = vec3(r.wSum, r.M, r.W);
}
