#version 450 core

#define PI 3.1415926

in VSOutput
{
	vec2 texCoord0;
} psIn;

layout(location = 0) out vec3 ssao;
layout(location = 1) out vec3 ssbn;
layout(location = 2) out vec3 outIrradiance;

uniform sampler2D depthBuffer;
uniform sampler2D normalBuffer;
uniform sampler2D HiZ;
uniform sampler2D directLightingBuffer;
uniform sampler2D blueNoiseTexture;
uniform int numLevels;
uniform int kMaxNumIterations;
uniform vec2 outputSize;

layout(std430) buffer ViewBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
};

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
* todo: my implementation seems to produce banding artifacts while the borrowed version
* looks right. Haven't figure out why this is the case. Maybe should reference the "raytracing in one
* weekend" books to look for how the author transform sample vector from local(tangent) space to world space.
*/
mat3 tangentToWorld(vec3 n)
{
	vec3 worldUp = abs(n.y) < 0.99f ? vec3(0.f, 1.f, 0.f) : vec3(0.f, 0.f, -1.f);
	vec3 right = cross(worldUp, n);
	vec3 forward = cross(n, right);
	mat3 coordFrame = {
		right,
		forward,
		n
	};
	return coordFrame;
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

bool hierarchicalTrace(in vec3 worldSpaceRO, in vec3 worldSpaceRD, inout float t)
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
            tDepth = minDepth - pp.z;
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
        // deal with the edge case where floor() == ceil()
        if (boundry.x == boundry.y)
        {
            boundry.x -= 1.f;
        }
        if (boundry.z == boundry.w)
        {
            boundry.z -= 1.f;
        }
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
			if (level <= 0)
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

// todo: improve this by applying spatiotemporal reuse
void calcAmbientOcclusionAndBentNormal(vec3 p, vec3 n, inout float ao, inout vec3 bentNormal, inout vec3 irradiance) 
{
    int numOccludedRays = 0;
	const int kNumRays = 16;
    for (int ray = 0; ray < kNumRays; ++ray)
    {
		float randomRotation = texture(blueNoiseTexture, gl_FragCoord.xy / float(textureSize(blueNoiseTexture, 0).xy)).r * PI * 2.f;
		vec3 rd = normalize(blueNoiseCosWeightedSampleHemisphere(n, BlueNoiseInDisk[ray], randomRotation));
        // vec3 rd = uniformSampleHemisphere(n);

        float t;
        bool bHit = hierarchicalTrace(p, rd, t);
        if (bHit)
        {
            numOccludedRays++;
            // accumulate indirect lighting
            vec3 worldSpaceHitPos = p + t * rd;
            // project to screen space and then sample the direct lighting buffer
            vec3 screenSpaceHitPos = worldToScreen(worldSpaceHitPos, view, projection);
            vec3 normal = normalize(texture(normalBuffer, screenSpaceHitPos.xy).xyz * 2.f - 1.f);
            float ndotl0 = max(dot(normal, -rd), 0.f);
            float ndotl1 = max(dot(n, rd), 0.f);
            irradiance += texture(directLightingBuffer, screenSpaceHitPos.xy).rgb * ndotl0 * ndotl1;
        }
        else 
        {
			bentNormal += rd;
        }
	}

	ao = float(numOccludedRays) / float(kNumRays);
	bentNormal = normalize(bentNormal);
    irradiance /= float(kNumRays);
};

void calcDiffuseGI(inout float ao, inout vec3 bentNormal, inout vec3 irradiance)
{
    
}

void main()
{	
    seed = 0;
    flat_idx = int(floor(gl_FragCoord.y) * outputSize.x + floor(gl_FragCoord.x));

    float depth = texture(depthBuffer, psIn.texCoord0).r;
    vec3 normal = texture(normalBuffer, psIn.texCoord0).xyz;
    normal = normalize(normal * 2.f - 1.f);

    if (depth > .9999f) 
		discard;

    // x, y, z in [0, 1]
    vec3 screenSpaceRO = vec3(psIn.texCoord0, depth);
	vec3 worldSpaceRO = screenToWorld(vec3(psIn.texCoord0, depth) * 2.f - 1.f, inverse(view), inverse(projection));

	vec3 viewDirection = normalize(-(view * vec4(worldSpaceRO, 1.f)).xyz);
	vec3 worldSpaceViewDirection = (inverse(view) * vec4(viewDirection, 0.f)).xyz;

    float ao = 1.f;
    vec3 bentNormal = worldSpaceViewDirection;
    vec3 irradiance = vec3(0.f);
	calcAmbientOcclusionAndBentNormal(worldSpaceRO, normal, ao, bentNormal, irradiance);
    ssao = vec3(1.f - ao);
    ssbn = bentNormal * .5f + .5f;
    outIrradiance = irradiance;
}
