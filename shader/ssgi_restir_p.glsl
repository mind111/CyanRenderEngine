#version 450 core

#define PI 3.1415926

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outIndirectIrradiance;

layout(std430) buffer ViewBuffer
{
    mat4  view;
    mat4  projection;
};

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

uniform int numLevels;
uniform int kMaxNumIterations;
uniform sampler2D HiZ;

bool HiZTrace(in vec3 worldSpaceRO, in vec3 worldSpaceRD, inout float t)
{
    bool bHit = false;

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

    // slightly offset the ray in screen space to avoid self intersection
    screenSpaceT += 0.001f;
// todo: review the rest of this tracing procedure
    int level = numLevels - 1;
    for (int i = 0; i < kMaxNumIterations; ++i)
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

		// calculate height field intersection
		float minDepth = textureLod(HiZ, pp.xy, level).r;
		float tDepth;
        if (minDepth <= pp.z)
        {
            tDepth = (minDepth - pp.z);
        }
        else
        {
			tDepth = (minDepth - pp.z) / rd.z;
			tDepth = (tDepth < 0.f) ? (1.f / 0.f) : tDepth;
        }

		// calculate cell boundries intersection
		vec2 mipSize = vec2(textureSize(HiZ, level));
		vec2 texelSize = 1.f / mipSize;
		vec2 coord = pp.xy / texelSize;
        vec4 boundry = vec4(floor(coord.x), ceil(coord.x), floor(coord.y), ceil(coord.y));

		float left = boundry.x * texelSize.x;
		float right = boundry.y * texelSize.x;
		float bottom = boundry.z * texelSize.y;
		float top = boundry.w * texelSize.y;

		float tCellBoundry = 1.f / 0.f;
        float tl = (left - pp.x) / rd.x;
        tCellBoundry = (tl > 0.f) ? min(tCellBoundry, tl) : tCellBoundry;
        float tr = (right - pp.x) / rd.x;
        tCellBoundry = (tr > 0.f) ? min(tCellBoundry, tr) : tCellBoundry;
        float tb = (bottom - pp.y) / rd.y;
        tCellBoundry = (tb > 0.f) ? min(tCellBoundry, tb) : tCellBoundry;
        float tt = (top - pp.y) / rd.y;
        tCellBoundry = (tt > 0.f) ? min(tCellBoundry, tt) : tCellBoundry;

		if (tDepth <= tCellBoundry)
		{
            // we find a good enough hit
			if (level <= 1)
            {
                if (tDepth > 0.f)
                {
					screenSpaceT += tDepth;
                }
                bHit = true;
                break;
            }
			// go down a level to perform more detailed trace
            level = max(level - 1, 0);
		}
		else
		{
            // bump the ray a tiny bit to avoid having it landing exactly on the texel boundry at current level
            float rayBump = 0.25f * length(rd.xy) * 1.f / min(textureSize(HiZ, 0).x, textureSize(HiZ, 0).y);
			// go up a level to perform more coarse trace
			screenSpaceT += (tCellBoundry + rayBump);
            level = min(level + 1, numLevels - 1);
		}
    }

    if (bHit)
    {
        vec3 screenSpaceHitPos = ro + screenSpaceT * rd;
        vec3 worldSpaceHitPos = screenToWorld(screenSpaceHitPos * 2.f - 1.f, inverse(view), inverse(projection));
        t = length(worldSpaceHitPos - worldSpaceRO);
    }
    return bHit;
}

struct Reservoir
{
    vec3 sampleRadiance;
    // vec3 samplePosition;
    // vec3 sampleNormal;
    float wSum;
    float M;
    float W;
};

float calcLuminance(vec3 inLinearColor)
{
    return 0.2126 * inLinearColor.r + 0.7152 * inLinearColor.g + 0.0722 * inLinearColor.b;
}

void updateReservoir(inout Reservoir r, in vec3 radiance, in float wi)
{
    r.wSum += wi;
    float p = wi / r.wSum;
    if (get_random().x < p)
    {
		r.sampleRadiance = radiance;
        // r.sampleNormal = normal;
        // r.samplePosition = position;
    }

    r.M += 1.f;
    float pdf = calcLuminance(r.sampleRadiance);
    r.W = r.wSum / (pdf * r.M);
}

void mergeReservoir()
{
}

// todo: write a shader #include thing
// #import "random.csh"

uniform sampler2D sceneDepthBuffer;
uniform sampler2D sceneNormalBuffer;
uniform sampler2D diffuseRadianceBuffer;

// note: for now, assuming that 
uniform sampler2D reservoirRadiance;
uniform sampler2D reservoirPosition;
uniform sampler2D reservoirWandM;

void main()
{
	float deviceDepth = texture(sceneDepthBuffer, psIn.texCoord0).r; 
	vec3 n = texture(sceneNormalBuffer, psIn.texCoord0).rgb * 2.f - 1.f;

	vec3 ro = screenToWorld(vec3(psIn.texCoord0, deviceDepth) * 2.f - 1.f, inverse(view), inverse(projection));

    Reservoir r;
    r.sampleRadiance = vec3(0.f);
    r.wSum = 0.f;
    r.M = 0.f;
    r.W = 0.f;

#if 1
    const int numSamples = 16;
    for (int i = 0; i < numSamples; ++i)
    {
		vec3 rd = uniformSampleHemisphere(n);
		float t;
        vec3 indirectRadiance = vec3(0.f);
		if (HiZTrace(ro + n * 0.001f, rd, t))
		{
			vec3 hitPosition = ro + t * rd;
			vec2 screenCoord = worldToScreen(hitPosition, view, projection).xy;
			indirectRadiance = texture(diffuseRadianceBuffer, screenCoord).rgb; 
		}

		// evaluate target pdf
		float targetPdf = calcLuminance(indirectRadiance);
		float wi = targetPdf / 1.f;
		updateReservoir(r, indirectRadiance, wi);
    }
#endif

    vec3 indirectIrradianceEstimate = r.sampleRadiance * r.W;
    outIndirectIrradiance = indirectIrradianceEstimate;
}
