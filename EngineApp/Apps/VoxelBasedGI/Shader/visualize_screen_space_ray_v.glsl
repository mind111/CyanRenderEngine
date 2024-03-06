#version 450 core

#define PI 3.1415926

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

 out VSOutput
{
	vec3 ro;
	vec3 re;
	float cull;
} vsOut;

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
uniform float u_freezeUpdating;
uniform sampler2D u_sceneDepthTex;
uniform sampler2D u_sceneNormalTex;

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) 
{
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
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

struct HierarchicalZBuffer
{
	sampler2D depthQuadtree;
	int numMipLevels;
};

uniform HierarchicalZBuffer hiz;
struct Settings
{
	int kMaxNumIterationsPerRay;
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
	float continueT; // this is in world space
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
    vec4 ssRayEnd = projection * view * vec4(worldSpaceRO + worldSpaceRD, 1.f);
    ssRayEnd /= ssRayEnd.w;
    // todo: assuming that perspective division maps depth range to [-1, 1], need to verify
    ssRayEnd = ssRayEnd * .5f + .5f;

    // parameterize ro and rd so that it's a ray that goes from the near plane to the far plane in NDC space
    vec3 rd = ssRayEnd.xyz - screenSpaceRO;
    rd /= abs(rd.z);
	float maxT = abs(ssRayEnd.z - screenSpaceRO.z);

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
	float lastStepT = 0.f;

	int level = startLevel;
    for (int i = 0; i < settings.kMaxNumIterationsPerRay; ++i)
    {
        // ray reached near/far plane and no intersection found
		#if 0
		if (screenSpaceT >= maxT)
        {
			break;
        }
		#endif

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
		lastStepT = tCell;
    }

	vec3 screenSpaceHitPos = ro + screenSpaceT * rd;
	vec3 derivedWorldSpaceHitPos = screenToWorld(screenSpaceHitPos * 2.f - 1.f, inverse(view), inverse(projection));
	vec3 prevScreenSpacePos = ro + (screenSpaceT - lastStepT) * rd;
	vec3 prevWorldSpacePos = screenToWorld(prevScreenSpacePos * 2.f - 1.f, inverse(view), inverse(projection));

    if (bHit)
    {
		hitRecord.result = HitResult_Hit;

		float depth = texture(u_sceneDepthTex, screenSpaceHitPos.xy).r;
		vec3 actualWorldSpaceHitPos = screenToWorld(vec3(screenSpaceHitPos.xy, depth) * 2.f - 1.f, inverse(view), inverse(projection));

		// ray hidden by depth buffer, not sure if we have a hit or not
		float hitPosError = length(derivedWorldSpaceHitPos - actualWorldSpaceHitPos);
		if (hitPosError >= 0.05f)
		{
			hitRecord.result = HitResult_Hidden;
			hitRecord.continueT = length(prevWorldSpacePos - worldSpaceRO);
		}
		else
		{
			// backface hit
			vec3 hitNormal = texture(u_sceneNormalTex, screenSpaceHitPos.xy).rgb * 2.f - 1.f;
			if (dot(hitNormal, -worldSpaceRD) <= 0.f)
			{
				hitRecord.result = HitResult_BackfaceHit;
				hitRecord.continueT = length(prevWorldSpacePos - worldSpaceRO);
			}
			// found a valid hit
			else
			{
				hitRecord.result = HitResult_Hit;
				hitRecord.continueT = 0.f;
			}
		}

		hitRecord.positionWS = actualWorldSpaceHitPos;
		hitRecord.positionSS = screenSpaceHitPos.xy;
		vec3 hitNormal = texture(u_sceneNormalTex, screenSpaceHitPos.xy).rgb * 2.f - 1.f;
		hitRecord.normalWS = hitNormal;
    }
	else
	{
		// if miss
		hitRecord.continueT = length(prevWorldSpacePos - worldSpaceRO);
	}
}

void main()
{
	seed = 0;
	flat_idx = 0;
	vsOut.cull = 0.f;

	const vec2 coord = vec2(.5f);
	float depth = texture(u_sceneDepthTex, coord).r;
	if (depth >= 0.999999f)
	{
		vsOut.cull = 1.f;
		return;
	}
	vec3 n = normalize(texture(u_sceneNormalTex, coord).rgb * 2.f - 1.f);
	vec3 worldSpacePosition = screenToWorld(vec3(coord, depth) * 2.f - 1.f, inverse(viewParameters.viewMatrix), inverse(viewParameters.projectionMatrix));

	vec3 ro = worldSpacePosition;
	vec3 rd = n;
	// vec3 rd = uniformSampleHemisphere(n);

	#if 0
	HitRecord ssHit;
	hizTrace(ro, rd, ssHit);

	vsOut.ro = worldSpacePosition;
	if (ssHit.result == HitResult_Hit)
	{
		vsOut.re = ssHit.positionWS;
	}
	else
	{
		vsOut.re = ro + rd * ssHit.continueT;
	}
	#else
	vsOut.ro = worldSpacePosition;
	vsOut.re = worldSpacePosition + n;
	#endif

	gl_Position = vec4(worldSpacePosition, 1.f);
}
