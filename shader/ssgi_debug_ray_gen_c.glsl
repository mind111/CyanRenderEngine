#version 450 core

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

#define PI 3.1415926

layout(std430) buffer DebugRayBuffer
{
	vec4 ro;
	vec4 screenSpaceRo;
	vec4 screenSpaceRoWithOffset;
	vec4 screenSpaceRoOffsetT;
	vec4 screenSpaceT;
	vec4 rd;
	vec4 screenSpaceRd;
	vec4 n;
	vec4 screenSpaceHitPosition;
	vec4 worldSpaceHitPosition;
	vec4 hitRadiance;
	vec4 hitNormal;
	vec4 hitResult;
} debugRayBuffer;

struct RayMarchingInfo
{
	vec4 screenSpacePosition;
	vec4 worldSpacePosition;
	vec4 depthSampleCoord;
	vec4 mipLevel;
	vec4 minDepth;
	vec4 tCell;
	vec4 tDepth;
};

layout(std430) buffer RayMarchingInfoBuffer
{
	RayMarchingInfo infos[];
} rayMarchingInfoBuffer;

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
uniform sampler2D diffuseRadianceBuffer;
uniform vec2 debugRayScreenCoord;

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
};
uniform Settings settings;

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

float offsetAlongRay(in vec2 p, in vec3 rd, float cellSize)
{
	const float epsilon = .25f;
	float rdLengthInXY = length(rd.xy);
	vec3 normalizedRd = normalize(rd);
	// offset p slightly to avoid it sitting on the boundry
	float offset = epsilon * cellSize;
	return offset / rdLengthInXY;
}

float calcIntersectionWithDepth(in vec3 p, in vec3 rd, sampler2D depthQuadtree, int level, inout RayMarchingInfo info)
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
	info.depthSampleCoord = vec4(sampleCoord * 2.f - 1.f, 0.f, 1.f);
	info.minDepth = vec4(minDepth);
	info.tDepth = vec4(tDepth);
	return tDepth;
}

/**
* this avoid the case where the ray got stuck on the quadtree cell boundry 
*/
 float calcIntersectionWithQuadtreeCell(in vec2 p, in vec3 rd, int numCells, inout RayMarchingInfo info)
 {
	float cellSize = 1.f / float(numCells);

	// slightly offset the p to avoid it sitting on the boundry
	float offset = offsetAlongRay(p, rd, cellSize);
	vec2 pp = p + rd.xy * offset;

	float rdLengthInXY = length(rd.xy);
	vec3 normalizedRd = normalize(rd);

	// determine if p is on cell boundry
	float l = floor(p.x / cellSize) * cellSize;
	float r = ceil(p.x / cellSize) * cellSize;
	float b = floor(p.y / cellSize) * cellSize;
	float t = ceil(p.y / cellSize) * cellSize;

	float tl = (l - p.x) / normalizedRd.x;
	float tr = (r - p.x) / normalizedRd.x;
	float tb = (b - p.y) / normalizedRd.y;
	float tt = (t - p.y) / normalizedRd.y;

	const float largeNumber = 1e+5;
	const float smallNumber = 1e-6;
	float tCell = largeNumber;
	tCell = (tl > 0.f) ? min(tCell, tl) : tCell;
	tCell = (tr > 0.f) ? min(tCell, tr) : tCell;
	tCell = (tb > 0.f) ? min(tCell, tb) : tCell;
	tCell = (tt > 0.f) ? min(tCell, tt) : tCell;
	tCell *= 1.f / rdLengthInXY; 

	info.tCell = vec4(tCell + offset);
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
	debugRayBuffer.screenSpaceRd = vec4(rd, 0.f);

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

	RayMarchingInfo dummyInfo;
	const int startLevel = 2;
	float rayOriginOffsetT = calcIntersectionWithQuadtreeCell(ro.xy, rd, int(textureSize(hiz.depthQuadtree, startLevel + 2).x), dummyInfo);
	screenSpaceT += rayOriginOffsetT;
	float marchedT = rayOriginOffsetT;
	debugRayBuffer.screenSpaceRoOffsetT = vec4(rayOriginOffsetT);

	vec3 roWithOffset = ro + screenSpaceT * rd;
	debugRayBuffer.screenSpaceRo = vec4(screenSpaceRO.xy * 2.f - 1.f, screenSpaceRO.z, 1.f);
	debugRayBuffer.screenSpaceRoWithOffset = vec4(roWithOffset.xy * 2.f - 1.f, roWithOffset.z, 1.f);

	int level = 2;
    for (int i = 0; i < settings.kMaxNumIterationsPerRay; ++i)
    {
        // ray reached near/far plane and no intersection found
		if (screenSpaceT >= 1.f)
        {
			break;
        }

        // ray marching 
		vec3 pp = ro + screenSpaceT * rd;

		RayMarchingInfo info;
		vec3 ppWorldSpace = screenToWorld(pp * 2.f - 1.f, inverse(view), inverse(projection));
		info.worldSpacePosition = vec4(ppWorldSpace, 1.f);

        // ray goes outside of the viewport and no intersection found
        if (pp.x < 0.f || pp.x > 1.f || pp.y < 0.f || pp.y > 1.f)
        {
			break;
        }

		float tDepth = calcIntersectionWithDepth(pp, rd, hiz.depthQuadtree, level, info);
		float tCell = calcIntersectionWithQuadtreeCell(pp.xy, rd, int(textureSize(hiz.depthQuadtree, level).x), info);

		info.screenSpacePosition = vec4(pp.xy * 2.f - 1.f, pp.z, 1.f);
		info.mipLevel = vec4(level);
		rayMarchingInfoBuffer.infos[i] = info;

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
	debugRayBuffer.screenSpaceT = vec4(screenSpaceT);

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

void main()
{
    seed = 0;
	vec2 fragCoord = debugRayScreenCoord * viewParameters.renderResolution;
    flat_idx = int(fragCoord.y * viewParameters.renderResolution.x + fragCoord.x);

	float deviceDepth = texture(sceneDepthBuffer, debugRayScreenCoord).r; 
	vec3 n = texture(sceneNormalBuffer, debugRayScreenCoord).rgb * 2.f - 1.f;
	debugRayBuffer.n = vec4(n, 0.f);

    if (deviceDepth <= 0.999999f)
	{
		mat4 view = viewParameters.viewMatrix;
		mat4 projection = viewParameters.projectionMatrix;
		vec3 ro = screenToWorld(vec3(debugRayScreenCoord, deviceDepth) * 2.f - 1.f, inverse(view), inverse(projection));
		vec3 rd = uniformSampleHemisphere(n);
		float t;
		vec2 screenSpaceHit;
		HitRecord hit;
		hizTrace(ro, rd, hit);
		if (hit.result == HitResult_Hit)
		{
			debugRayBuffer.hitRadiance = texture(diffuseRadianceBuffer, hit.positionSS);
		}
		debugRayBuffer.worldSpaceHitPosition = vec4(hit.positionWS, 1.f);
		debugRayBuffer.screenSpaceHitPosition = vec4(hit.positionSS * 2.f - 1.f, 0.f, 1.f);
		debugRayBuffer.hitNormal = vec4(hit.normalWS, 0.f);
		debugRayBuffer.hitResult = vec4(hit.result);
		debugRayBuffer.ro = vec4(ro, 1.f);
		debugRayBuffer.rd = vec4(rd, 0.f);
	}
}
