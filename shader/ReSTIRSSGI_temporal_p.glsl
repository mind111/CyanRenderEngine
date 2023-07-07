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

/* note: 
* tangent to world space transform taken from https://www.shadertoy.com/view/4lfcDr
*/
mat3 construct_ONB_frisvad(vec3 normal)
{
	mat3 ret;
	ret[2] = normal;
    // if normal.z == -1.f
	if(normal.z < -0.999805696) {
		ret[0] = vec3(0.0, -1.0, 0.0);
		ret[1] = vec3(-1.0, 0.0, 0.0);
	}
	else {
		float a = 1.0 / (1.0 + normal.z);
		float b = -normal.x * normal.y * a;
		ret[0] = vec3(1.0 - normal.x * normal.x * a, b, -normal.x);
		ret[1] = vec3(b, 1.0 - normal.y * normal.y * a, -normal.y);
	}
	return ret;
}

vec3 sphericalToCartesian(float theta, float phi, vec3 n)
{
	vec3 localDir = {
		sin(theta) * cos(phi),
		sin(theta) * sin(phi),
		cos(theta)
	};
	return construct_ONB_frisvad(n) * localDir;
}

vec3 uniformSampleHemisphere(vec3 n)
{
    vec2 uv = get_random();
	float theta = acos(uv.x);
	float phi = 2 * PI * uv.y;
	return normalize(sphericalToCartesian(theta, phi, n));
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

struct Reservoir
{
    vec3 radiance;
    vec3 position;
    vec3 normal;
    float wSum;
    float M;
    float W;
};

float calcLuminance(vec3 inLinearColor)
{
    return 0.2126 * inLinearColor.r + 0.7152 * inLinearColor.g + 0.0722 * inLinearColor.b;
}

void updateReservoir(inout Reservoir r, in vec3 radiance, in vec3 samplePosition, in vec3 sampleNormal, in float wi)
{
    r.wSum += wi;
    float p = wi / r.wSum;
    if (get_random().x < p)
    {
		r.radiance = radiance;
        r.position = samplePosition;
        r.normal = sampleNormal;
    }

    r.M += 1.f;

	float targetPdf = calcLuminance(r.radiance);
	if (targetPdf > 0.01f)
	{
		r.W = r.wSum / (targetPdf * r.M);
	}
	else
	{
		r.W = 0.f;
	}
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

uniform sampler2D sceneDepthBuffer;
uniform sampler2D sceneNormalBuffer;
uniform sampler2D diffuseRadianceBuffer;
uniform sampler2D prevFrameIndirectIrradianceBuffer;

uniform sampler2D prevFrameReservoirRadiance;
uniform sampler2D prevFrameReservoirPosition;
uniform sampler2D prevFrameReservoirNormal;
uniform sampler2D prevFrameReservoirWSumMW;
uniform sampler2D prevFrameSceneDepthBuffer;

void lookupTemporalReservoir(inout Reservoir r, in vec3 worldSpacePosition)
{
    // reproject
	// todo: take pixel velocity into consideration when doing reprojection
	// todo: consider changes in pixel neighborhood when reusing cache sample, changes in neighborhood pixels means SSAO value can change even there is a cache hit
	// todo: adaptive convergence-aware spatial filtering
	mat4 prevFrameView = viewParameters.prevFrameViewMatrix;
	mat4 prevFrameProjection = viewParameters.prevFrameProjectionMatrix;

	vec3 prevViewSpacePos = (prevFrameView * vec4(worldSpacePosition, 1.f)).xyz;
	vec4 prevNDCPos = prevFrameProjection * vec4(prevViewSpacePos, 1.f);
	prevNDCPos /= prevNDCPos.w;
	prevNDCPos.xyz = prevNDCPos.xyz * .5f + .5f;

	if (prevNDCPos.x <= 1.f && prevNDCPos.x >= 0.f && prevNDCPos.y <= 1.f && prevNDCPos.y >= 0.f)
	{
		float prevFrameDeviceZ = texture(prevFrameSceneDepthBuffer, prevNDCPos.xy).r;
		vec3 cachedPrevFrameViewSpacePos = (prevFrameView * vec4(screenToWorld(vec3(prevNDCPos.xy, prevFrameDeviceZ) * 2.f - 1.f, inverse(prevFrameView), inverse(prevFrameProjection)), 1.f)).xyz;
		float relativeDepthDelta = abs(cachedPrevFrameViewSpacePos.z - prevViewSpacePos.z) / -cachedPrevFrameViewSpacePos.z;
		// cache hit
        if (relativeDepthDelta < 0.01f)
        {
			r.radiance = texture(prevFrameReservoirRadiance, prevNDCPos.xy).rgb;
			r.position = texture(prevFrameReservoirPosition, prevNDCPos.xy).xyz;
			r.normal = texture(prevFrameReservoirNormal, prevNDCPos.xy).xyz * 2.f - 1.f;
			vec3 wSumMW = texture(prevFrameReservoirWSumMW, prevNDCPos.xy).xyz;
			r.wSum = wSumMW.x; 
			r.M = wSumMW.y; 
			r.W = wSumMW.z;
        }
	}
}

struct HierarchicalZBuffer
{
	sampler2D depthQuadtree;
	int numMipLevels;
};
uniform HierarchicalZBuffer hiz;

struct Settings 
{
	int kTracingStopMipLevel;
	int kMaxNumIterationsPerRay;
    float bNearFieldSSAO;
	float bSSDO;
    float bIndirectIrradiance;
	float indirectBoost;
};
uniform Settings settings;

float offsetAlongRay(in vec2 p, in vec3 rd, float cellSize)
{
	const float epsilon = .25f;
	float rdLengthInXY = length(rd.xy);
	vec3 normalizedRd = normalize(rd);
	// offset p slightly to avoid it sitting on the boundry
	float offset = epsilon * cellSize;
	return offset / rdLengthInXY;
}

float calcIntersectionWithDepth(in vec3 p, in vec3 rd, sampler2D depthQuadtree, int level)
{
	float numCells = textureSize(depthQuadtree, level).r;
	float cellSize = 1.f / numCells;

	// slightly offset the p to avoid it sitting on the boundry
	float offset = offsetAlongRay(p.xy, rd, cellSize);
	vec3 pp = p + rd * offset;

	// snap sample coord to texel center
	vec2 sampleCoord = (floor(pp.xy / cellSize) + .5f) / numCells;
	float tDepth;
	float minDepth = textureLod(depthQuadtree, sampleCoord, level).r;
	if (rd.z != 0.f)
	{
		tDepth = (minDepth - pp.z) / rd.z;
	}
	return tDepth;
}

 float calcIntersectionWithQuadtreeCell(in vec2 p, in vec3 rd, int numCells)
 {
	float cellSize = 1.f / float(numCells);

	// slightly offset the p to avoid it sitting on the boundry
	float offset = offsetAlongRay(p, rd, cellSize);
	vec2 pp = p + rd.xy * offset;

	float rdLengthInXY = length(rd.xy);
	vec3 normalizedRd = normalize(rd);

	// determine if p is on cell boundry
	float l = floor(pp.x / cellSize) * cellSize;
	float r = ceil(pp.x / cellSize) * cellSize;
	float b = floor(pp.y / cellSize) * cellSize;
	float t = ceil(pp.y / cellSize) * cellSize;

	float tl = (l - pp.x) / normalizedRd.x;
	float tr = (r - pp.x) / normalizedRd.x;
	float tb = (b - pp.y) / normalizedRd.y;
	float tt = (t - pp.y) / normalizedRd.y;

	const float largeNumber = 1e+5;
	const float smallNumber = 1e-6;
	float tCell = largeNumber;
	tCell = (tl > 0.f) ? min(tCell, tl) : tCell;
	tCell = (tr > 0.f) ? min(tCell, tr) : tCell;
	tCell = (tb > 0.f) ? min(tCell, tb) : tCell;
	tCell = (tt > 0.f) ? min(tCell, tt) : tCell;
	tCell *= 1.f / rdLengthInXY; 
	return tCell + offset;
 }

const int HitResult_Hit = 0;
const int HitResult_BackfaceHit = 1;
const int HitResult_Hidden = 2;
const int HitResult_Miss = 3;

struct HitRecord
{
	int result;
	vec3 positionWS; // world space hit position
	vec2 positionSS; // screen space hit position
	vec3 normalWS;
};

// todo: deal with the case when rd.z = 0
void hizTrace(in vec3 worldSpaceRO, in vec3 worldSpaceRD, inout HitRecord hitRecord)
{
    bool bHit = false;
	hitRecord.result = HitResult_Miss;
	mat4 view = viewParameters.viewMatrix;
	mat4 projection = viewParameters.projectionMatrix;

    vec4 clipSpaceRO = projection * view * vec4(worldSpaceRO, 1.f);
    clipSpaceRO /= clipSpaceRO.w;
    vec3 screenSpaceRO = clipSpaceRO.xyz * .5f + .5f;

    // project a point on along the ray into screen space
    vec4 screenSpacePos = projection * view * vec4(worldSpaceRO + worldSpaceRD, 1.f);
    screenSpacePos /= screenSpacePos.w;
    // todo: assuming that perspective division maps depth range to [-1, 1], need to verify
    screenSpacePos = screenSpacePos * .5f + .5f;

    // parameterize ro and rd so that it's a ray that goes from the near plane to the far plane in NDC space
    vec3 rd = screenSpacePos.xyz - screenSpaceRO;
    rd /= abs(rd.z);

    vec3 ro;
    float screenSpaceT;
    if (rd.z > 0.f)
    {
		ro = screenSpaceRO - rd * screenSpaceRO.z;
        screenSpaceT = screenSpaceRO.z;
    }
    else 
    {
		ro = screenSpaceRO - rd * (1.f - screenSpaceRO.z);
		screenSpaceT = (1.f - screenSpaceRO.z);
    }

	const int startLevel = 2;
	float rayOriginOffsetT = calcIntersectionWithQuadtreeCell(ro.xy, rd, int(textureSize(hiz.depthQuadtree, startLevel + 2).x));
	screenSpaceT += rayOriginOffsetT;
	float marchedT = rayOriginOffsetT;

	int level = startLevel;
    for (int i = 0; i < settings.kMaxNumIterationsPerRay; ++i)
    {
        // ray reached near/far plane and no intersection found
		if (screenSpaceT >= 1.f)
        {
			break;
        }

        // ray marching 
		vec3 pp = ro + screenSpaceT * rd;

        // ray goes outside of the viewport and no intersection found
        if (pp.x < 0.f || pp.x > 1.f || pp.y < 0.f || pp.y > 1.f)
        {
			break;
        }

		float tDepth = calcIntersectionWithDepth(pp, rd, hiz.depthQuadtree, level);
		float tCell = calcIntersectionWithQuadtreeCell(pp.xy, rd, int(textureSize(hiz.depthQuadtree, level).x));

		if (level <= 0)
		{
            // we find a good enough hit
			bHit = true;
			break;
		}

		if (rd.z > 0.f)
		{
			if (tDepth < 0.f || (tDepth > 0.f && tDepth <= tCell))
			{
				// go down a level to perform more detailed trace
				level = max(level - 1, 0);
				continue;
			} 
		}
		else
		{
			if (tDepth >= 0.f)
			{
				// go down a level to perform more detailed trace
				level = max(level - 1, 0);
				continue;
			}
		}

		screenSpaceT += tCell;
		marchedT += tCell;
		level = min(level + 1, hiz.numMipLevels - 1);
    }

    if (bHit)
    {
		hitRecord.result = HitResult_Hit;

        vec3 screenSpaceHitPos = ro + screenSpaceT * rd;
		vec3 derivedWorldSpaceHitPos = screenToWorld(screenSpaceHitPos * 2.f - 1.f, inverse(view), inverse(projection));
		float depth = texture(sceneDepthBuffer, screenSpaceHitPos.xy).r;
		vec3  actualWorldSpaceHitPos = screenToWorld(vec3(screenSpaceHitPos.xy, depth) * 2.f - 1.f, inverse(view), inverse(projection));

		// incorrect hit caused by ray origin offset
		if (marchedT <= rayOriginOffsetT)
		{
			hitRecord.result = HitResult_BackfaceHit;
		}
		else
		{
			// ray hidden by depth buffer, not sure if we have a hit or not
			float hitPosError = length(derivedWorldSpaceHitPos - actualWorldSpaceHitPos);
			if (hitPosError >= 0.1f)
			{
				hitRecord.result = HitResult_Hidden;
			}
			else
			{
				// backface hit
				vec3 hitNormal = texture(sceneNormalBuffer, screenSpaceHitPos.xy).rgb * 2.f - 1.f;
				if (dot(hitNormal, -worldSpaceRD) <= 0.f)
				{
					hitRecord.result = HitResult_BackfaceHit;
				}
				// found a valid hit
				else
				{
					hitRecord.result = HitResult_Hit;
				}
			}
		}
		hitRecord.positionWS = actualWorldSpaceHitPos;
		hitRecord.positionSS = screenSpaceHitPos.xy;
		vec3 hitNormal = texture(sceneNormalBuffer, screenSpaceHitPos.xy).rgb * 2.f - 1.f;
		hitRecord.normalWS = hitNormal;
    }
}

#define MAX_TEMPORAL_SAMPLE_COUNT 16

void main()
{
    seed = viewParameters.frameCount;
    flat_idx = int(floor(gl_FragCoord.y) * viewParameters.renderResolution.x + floor(gl_FragCoord.x));

	float deviceDepth = texture(sceneDepthBuffer, psIn.texCoord0).r; 
	vec3 n = texture(sceneNormalBuffer, psIn.texCoord0).rgb * 2.f - 1.f;

    if (deviceDepth > 0.999999f) discard;

	mat4 view = viewParameters.viewMatrix, projection = viewParameters.projectionMatrix;
	vec3 ro = screenToWorld(vec3(psIn.texCoord0, deviceDepth) * 2.f - 1.f, inverse(view), inverse(projection));

    Reservoir r;
	r.radiance = vec3(0.f);
	r.position = vec3(0.f);
	r.normal = vec3(0.f);
	r.wSum = 0.f;
	r.M = 0.f;
	r.W = 0.f;
    if (viewParameters.frameCount > 0)
    {
        // todo: do temporal reprojection properly 
        // load temporal reservoir
        lookupTemporalReservoir(r, ro);
    }

    const int numSamples = 1;
    vec3 indirectIrradiance = vec3(0.f);

    for (int i = 0; i < numSamples; ++i)
    {
        vec3 incidentRadiance = vec3(0.f);

		vec3 rd = uniformSampleHemisphere(n);
		HitRecord hit;
		hizTrace(ro, rd, hit);
        // make the default hit position really really far
        vec3 hitPosition = ro + 10000.f * rd;
        vec3 hitNormal = vec3(0.f);
		if (hit.result == HitResult_Hit)
		{
			incidentRadiance = texture(diffuseRadianceBuffer, hit.positionSS).rgb;
			// todo: debug infinite bounce 
			// incidentRadiance += texture(prevFrameIndirectIrradianceBuffer, hit.positionSS).rgb;
			hitPosition = hit.positionWS; 
			hitNormal = hit.normalWS;
		}

		float srcPdf = 1.f / (2.f * PI);
		float targetPdf = calcLuminance(incidentRadiance);
		float wi = targetPdf / srcPdf;

        // clamping max number of allowed temporal samples to help with faster convergence under motion
        if (r.M >= MAX_TEMPORAL_SAMPLE_COUNT)
        {
            // reduce the weight sum by average
            r.wSum -= r.wSum / MAX_TEMPORAL_SAMPLE_COUNT;
            r.M = MAX_TEMPORAL_SAMPLE_COUNT - 1;
        }

		updateReservoir(r, incidentRadiance, hitPosition, hitNormal, wi);
    }

    outReservoirRadiance = r.radiance;
    outReservoirPosition = r.position;
	// todo: this normal might be problematic for jacobian calculation during spatial reservoir merging
    outReservoirNormal = r.normal * .5f + .5f;
    outReservoirWSumMW = vec3(r.wSum, r.M, r.W);
}
