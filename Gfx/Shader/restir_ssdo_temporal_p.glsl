#version 450 core

#define PI 3.1415926

in VSOutput
{
	vec2 texCoord0;
} psIn;

layout (location = 0) out vec3 outReservoirSkyRadiance;
layout (location = 1) out vec3 outReservoirSkyDirection;
layout (location = 2) out vec3 outReservoirSkyWSumMW;
layout (location = 3) out vec3 outScreenSpaceHitPosition;
layout (location = 4) out vec3 outDebugScreenSpaceHitPosition;

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
uniform sampler2D u_prevFrameSceneDepthTex; 
uniform sampler2D u_sceneNormalTex; 
uniform sampler2D u_sceneAlbedoTex; 
uniform sampler2D u_sceneMetallicRoughnessTex; 
uniform samplerCube u_skyboxCubemap; 
uniform sampler2D u_BRDFLUT; 
uniform sampler2D u_prevFrameSkyIrradianceTex;
uniform sampler2D u_prevFrameReservoirSkyRadiance;
uniform sampler2D u_prevFrameReservoirSkyDirection;
uniform sampler2D u_prevFrameReservoirSkyWSumMW;

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

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

/**
    f0 is specular reflectance
*/
vec3 calcF0(vec3 albedo, float metallic) 
{
    vec3 dialectricF0 = vec3(.16f) * (.5f) * (.5f);
    vec3 conductorF0 = albedo;
    return mix(dialectricF0, conductorF0, metallic);
}

float GGX(float roughness, float ndoth) 
{
    float alpha = roughness;
    float alpha2 = alpha * alpha;
    float result = max(alpha2, 1e-6); // prevent the nominator goes to 0 when roughness equals 0
    float denom = ndoth * ndoth * (alpha2 - 1.f) + 1.f;
    result /= (PI * denom * denom); 
    return result;
}

vec3 fresnel(vec3 f0, float vdoth)
{
    float fresnelCoef = 1.f - vdoth;
    fresnelCoef = fresnelCoef * fresnelCoef * fresnelCoef * fresnelCoef * fresnelCoef;
    // f0: fresnel reflectance at incidence angle 0
    // f90: fresnel reflectance at incidence angle 90, f90 in this case is vec3(1.f) 
    vec3 f90 = vec3(1.f);
    return mix(f0, f90, fresnelCoef);
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
    return 0.5f / max((v + l), 1e-5);
}

vec3 CookTorranceBRDF(vec3 wi, vec3 wo, vec3 n, float roughness, vec3 f0)
{
    float remappedRoughness = roughness * roughness;

    float ndotv = saturate(dot(n, wo));
    float ndotl = saturate(dot(n, wi));
    vec3 h = normalize(wi + wo);
    float ndoth = saturate(dot(n, h));
    float vdoth = saturate(dot(wo, h));

    float D = GGX(remappedRoughness, ndoth);
    float V = V_SmithGGXHeightCorrelated(ndotv, ndotl, remappedRoughness);
    vec3 F = fresnel(f0, vdoth);
    return D * F * V;
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

/**
* using blue noise to do importance sampling hemisphere
*/
vec3 blueNoiseCosWeightedSampleHemisphere(vec3 n, vec2 uv, float randomRotation)
{
	// rotate input samples
	mat2 rotation = {
		{ cos(randomRotation), sin(randomRotation) },
		{ -sin(randomRotation), cos(randomRotation) }
	};
	uv = rotation * uv;

	// project points on a unit disk up to the hemisphere
	float z = sin(acos(length(uv)));
	return construct_ONB_frisvad(n) * normalize(vec3(uv.xy, z));
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

	float phi = 2 * PI * xi.x;
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
		float depth = texture(u_sceneDepthTex, screenSpaceHitPos.xy).r;
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
				vec3 hitNormal = texture(u_sceneNormalTex, screenSpaceHitPos.xy).rgb * 2.f - 1.f;
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
		vec3 hitNormal = texture(u_sceneNormalTex, screenSpaceHitPos.xy).rgb * 2.f - 1.f;
		hitRecord.normalWS = hitNormal;
    }
}

#define SKY_DIST 1e5

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

float calcLuminance(vec3 inLinearColor)
{
    return 0.2126 * inLinearColor.r + 0.7152 * inLinearColor.g + 0.0722 * inLinearColor.b;
}


Reservoir createEmptyReservoir()
{
	Reservoir r;
	r.z.radiance = vec3(0.f);
	r.z.direction = vec3(0.f);
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

// todo: should srcPdf be 1.f or 1.f / 2.f * PI ...?
/**
 * this assumes that 
 */
float calcReSTIRGISampleWeight(in ReservoirSample s, float ndotl)
{
	float targetPdf = calcLuminance(s.radiance) * ndotl;
	float srcPdf = 1.f;
	return targetPdf / srcPdf;
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

bool isValidScreenCoord(vec2 coord)
{
	return (coord.x >= 0.f && coord.x <= 1.f && coord.y >= 0.f && coord.y <= 1.f); 
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
			r.z.radiance = texture(u_prevFrameReservoirSkyRadiance, reprojectedCoord.xy).rgb;
			r.z.direction = texture(u_prevFrameReservoirSkyDirection, reprojectedCoord.xy).xyz;
			vec3 wSumMW = texture(u_prevFrameReservoirSkyWSumMW, reprojectedCoord.xy).xyz;
			r.wSum = wSumMW.x; 
			r.M = wSumMW.y; 
			r.W = wSumMW.z;
		}
	}

	return lookupResult;
}

#define MAX_TEMPORAL_SAMPLE_COUNT 32

void main()
{
    seed = viewParameters.frameCount;
    flat_idx = int(floor(gl_FragCoord.y) * viewParameters.renderResolution.x + floor(gl_FragCoord.x));

	float depth = texture(u_sceneDepthTex, psIn.texCoord0).r; 
	vec3 n = texture(u_sceneNormalTex, psIn.texCoord0).rgb * 2.f - 1.f;

    if (depth > 0.999999f) discard;

	vec3 albedo = texture(u_sceneAlbedoTex, psIn.texCoord0).rgb;
	vec3 MRO = texture(u_sceneMetallicRoughnessTex, psIn.texCoord0).rgb;
	float metallic = MRO.r;
	float roughness = MRO.g;

	mat4 view = viewParameters.viewMatrix;
	mat4 prevFrameView = viewParameters.prevFrameViewMatrix;
	mat4 projection = viewParameters.projectionMatrix;
	mat4 prevFrameProjection = viewParameters.prevFrameProjectionMatrix;

	vec3 ro = screenToWorld(vec3(psIn.texCoord0, depth) * 2.f - 1.f, inverse(view), inverse(projection));
	vec3 worldSpaceViewDirection = normalize(viewParameters.cameraPosition - ro);

	vec3 rd = uniformSampleHemisphere(n);
	#if 0
    vec2 rng2 = get_random().xy;
    float angle = rng2.x * PI * 2.f;
	vec3 rd = blueNoiseCosWeightedSampleHemisphere(n, BlueNoiseInDisk[viewParameters.frameCount % 64], angle);
	#endif

	HitRecord hit;
	hizTrace(ro, rd, hit);
	float ndotl = max(dot(rd, n), 0.f);

	outScreenSpaceHitPosition = rd;

	// make the default hit position really really far as if it hits the sky
	vec3 hitPosition = SKY_DIST * rd;
	vec3 hitNormal = -rd;

	ReservoirSample s;
	s.radiance = vec3(0.f);
	s.direction = rd;
	Reservoir r = createFreshReservoirWithSample(s, calcReSTIRGISampleWeight(s, 0.f));

	if (hit.result == HitResult_Miss)
	{
		s.radiance = textureLod(u_skyboxCubemap, rd, 5.f).rgb;
		s.direction = rd;
		float ndotl = max(dot(n, rd), 0.f);
		r = createFreshReservoirWithSample(s, calcReSTIRGISampleWeight(s, ndotl));
	}
	else if (hit.result == HitResult_Hit)
	{
		// cache screen space hit position and defer radiance fetching
		outScreenSpaceHitPosition = vec3(hit.positionSS, 2.f);
	}

	// temporal reservoir reprojection
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
			float ndotl = max(dot(n, rd), 0.f);
			updateReservoir(rHistoryCenter, r.z, calcReSTIRGISampleWeight(r.z, ndotl));
			r = rHistoryCenter;
		}
	}

	outReservoirSkyRadiance = r.z.radiance;
	outReservoirSkyDirection = r.z.direction;
	outReservoirSkyWSumMW = vec3(r.wSum, r.M, r.W);
}
