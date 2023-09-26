#version 450 core

#define PI 3.1415926

in VSOutput
{
	vec2 texCoord0;
} psIn;

layout (location = 0) out vec3 outSkyIrradiance;
layout (location = 1) out vec3 outSkyReflection;
layout (location = 2) out vec3 outDirectLighting;
layout (location = 3) out vec3 outDirectDiffuseLighting;

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

	vec3 skyIrradiance = vec3(0.f);
	vec3 skyReflection = vec3(0.f);

	vec3 ro = screenToWorld(vec3(psIn.texCoord0, depth) * 2.f - 1.f, inverse(view), inverse(projection));
	vec3 worldSpaceViewDirection = normalize(viewParameters.cameraPosition - ro);

	// diffuse
	{
		vec3 rd = uniformSampleHemisphere(n);
		HitRecord hit;
		hizTrace(ro, rd, hit);

		// make the default hit position really really far as if it hits the sky
		vec3 hitPosition = SKY_DIST * rd;
		vec3 hitNormal = -rd;

		if (hit.result == HitResult_Miss)
		{
			float ndotl = max(dot(n, rd), 0.f);
			skyIrradiance += textureLod(u_skyboxCubemap, rd, 5.f).rgb * ndotl;
		}
	}

	// reflection (no occlusion)
	{
		vec3 f0 = calcF0(albedo, metallic);
		vec3 reflectionDirection = -reflect(worldSpaceViewDirection, n);
		vec3 incidentRadiance = textureLod(u_skyboxCubemap, reflectionDirection, roughness * log2(textureSize(u_skyboxCubemap, 0).x)).rgb;
		float ndotv = clamp(dot(n, worldSpaceViewDirection), 0.f, 1.f);
		vec3 BRDF = texture(u_BRDFLUT, vec2(ndotv, roughness)).rgb; 
		skyReflection += incidentRadiance * (f0 * BRDF.r + BRDF.g);
	}

    if (viewParameters.frameCount > 0)
    {
		// temporal reprojection 
		// todo: take pixel velocity into consideration when doing reprojection
		// todo: consider changes in pixel neighborhood when reusing cache sample, changes in neighborhood pixels means SSAO value can change even there is a cache hit
		// todo: adaptive convergence-aware spatial filtering
		vec3 prevViewSpacePos = (prevFrameView * vec4(ro, 1.f)).xyz;
		vec4 prevNDCPos = prevFrameProjection * vec4(prevViewSpacePos, 1.f);
		prevNDCPos /= prevNDCPos.w;
		prevNDCPos.xyz = prevNDCPos.xyz * .5f + .5f;

		if (prevNDCPos.x <= 1.f && prevNDCPos.x >= 0.f && prevNDCPos.y <= 1.f && prevNDCPos.y >= 0.f)
		{
			float prevFrameDeviceZ = texture(u_prevFrameSceneDepthTex, prevNDCPos.xy).r;
			vec3 cachedPrevFrameViewSpacePos = (prevFrameView * vec4(screenToWorld(vec3(prevNDCPos.xy, prevFrameDeviceZ) * 2.f - 1.f, inverse(prevFrameView), inverse(prevFrameProjection)), 1.f)).xyz;
			float relativeDepthDelta = abs(cachedPrevFrameViewSpacePos.z - prevViewSpacePos.z) / -cachedPrevFrameViewSpacePos.z;

			vec3 skyIrradianceHistory = texture(u_prevFrameSkyIrradianceTex, prevNDCPos.xy).rgb;

			float smoothing = .9f;
			smoothing = clamp(smoothing * 0.05f / relativeDepthDelta, 0.f, smoothing);
			// based on how fast the camera/object moves to further refine the smoothing, maybe it's better to use screen space displacement instead
			vec3 cameraDisplacement = view[3].xyz - prevFrameView[3].xyz;
			smoothing = mix(smoothing, 0.f, clamp(length(cameraDisplacement) / 0.1f, 0.f, 1.f));

			skyIrradiance = vec3(skyIrradianceHistory * smoothing + skyIrradiance * (1.f - smoothing));
		}
	}

	outSkyIrradiance = skyIrradiance;
	outSkyReflection = skyReflection;

	vec3 diffuseColor = (1.f - metallic) * albedo;
	outDirectLighting = diffuseColor * skyIrradiance + skyReflection;
	outDirectDiffuseLighting = diffuseColor * skyIrradiance;
}
