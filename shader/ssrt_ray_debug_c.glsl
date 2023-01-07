#version 450 core
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

#define PI 3.1415926

uniform vec2 debugCoord;
uniform sampler2D depthBuffer;
uniform sampler2D normalBuffer;
uniform sampler2D HiZ;
uniform sampler2D blueNoiseTexture;
uniform int numLevels;
uniform int kMaxNumIterations;
uniform int numRays;
uniform vec2 outputSize;

layout(std430) buffer ViewBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
};

struct Point
{
    vec4 position;
    vec4 color;
};

layout (std430) buffer DebugRayBuffer
{
    Point pointsAlongRay[];
};

struct DebugTraceData
{
	float minDepth; 
	float stepDepth; 
	float tDepth; 
	float tCellBoundry;
	float t; 
	int level; 
    int iteration;
    int stepCount;
	vec4 cellBoundry;
	vec2 cell;
    vec2 mipSize;
    vec4 pp;
    vec4 ro;
    vec4 rd;
};

layout (std430) buffer DebugTraceBuffer
{
    DebugTraceData debugTraces[];
};

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) 
{
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
}

mat3 tangentToWorld(vec3 n)
{
	vec3 worldUp = abs(n.y) < 0.99f ? vec3(0.f, 1.f, 0.f) : vec3(0.f, 0.f, -1.f);
	vec3 right = cross(worldUp, n);
	vec3 forward = cross(right, n);
	mat3 coordFrame = {
		right,
		forward,
		n
	};
	return coordFrame;
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
	return tangentToWorld(n) * normalize(vec3(uv.xy, z));
	// return tangentToWorld(vec3(0.f, 1.f, 0.f)) * normalize(vec3(uv.xy, z));
}

#if 0
bool hierarchicalTrace(in vec3 worldSpaceRO, in vec3 worldSpaceRD, inout float t, int rayID)
{
    bool bHit = false;

    // 
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
    if (rd.z >= 0.f)
    {
		ro = screenSpaceRO - rd * screenSpaceRO.z;
		t = screenSpaceRO.z;
    }
    else 
    {
		ro = screenSpaceRO - rd * (1.f - screenSpaceRO.z);
		t = (1.f - screenSpaceRO.z);
    }

    int level = numLevels - 1;
    int stepCount = 0;

	// clear debug buffer
	for (int i = 0; i < kMaxNumIterations; ++i)
	{
		debugTraces[i].minDepth = 0.f;
		debugTraces[i].stepDepth = 0.f;
		debugTraces[i].tDepth = 0.f;
		debugTraces[i].tCellBoundry = 0.f;
		debugTraces[i].t = 0.f;
		debugTraces[i].level = 0;
		debugTraces[i].iteration = 0;
		debugTraces[i].mipSize = vec2(0.f);
		debugTraces[i].cell = vec2(0.f);
		debugTraces[i].cellBoundry = vec4(0.f);
		debugTraces[i].pp = vec4(0.f);
		debugTraces[i].stepCount = 0;
		debugTraces[i].ro = vec4(ro, 1.f);
		debugTraces[i].rd = vec4(rd, 0.f);

		pointsAlongRay[rayID * kMaxNumIterations + i].position = vec4(0.f);
		pointsAlongRay[rayID * kMaxNumIterations + i].color = vec4(0.f);
	}

    for (int i = 0; i < kMaxNumIterations; ++i)
    {
        // ray reached near/far plane and no intersection found
		if (t > 1.f)
        {
			break;
        }

        // ray marching 
		vec3 pp = ro + t * rd;

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

		// record debug tracing data
		debugTraces[i].minDepth = minDepth;
		debugTraces[i].stepDepth = pp.z;
		debugTraces[i].tDepth = tDepth;
		debugTraces[i].tCellBoundry = tCellBoundry;
		debugTraces[i].t = t;
		debugTraces[i].level = level;
		debugTraces[i].iteration = i;
		debugTraces[i].mipSize = mipSize;
		debugTraces[i].cell = floor(coord);
		debugTraces[i].cellBoundry = vec4(left, right, bottom, top);
		debugTraces[i].pp = vec4(pp, 1.f);
		debugTraces[i].stepCount = stepCount;

		// unproject pp back to world space 
		pointsAlongRay[rayID * kMaxNumIterations + stepCount].position = vec4(screenToWorld(vec3(pp * 2.f - 1.f), inverse(view), inverse(projection)), 1.f);
		pointsAlongRay[rayID * kMaxNumIterations + stepCount].color = vec4(1.f, 0.f, 0.f, 1.f);

		if (tDepth <= tCellBoundry)
		{
            // we find a good enough hit
			if (level == 0)
            {
                if (tDepth > 0.f)
                {
					// t += tDepth;
					// record the hit point
					// pp = ro + t * rd;
					// pointsAlongRay[rayID * kMaxNumIterations + stepCount + 1].position = vec4(screenToWorld(vec3(pp * 2.f - 1.f), inverse(view), inverse(projection)), 1.f);
					// pointsAlongRay[rayID * kMaxNumIterations + stepCount + 1].color = vec4(0.f, 1.f, 0.f, 1.f);
                }
                else 
                {
					pointsAlongRay[rayID * kMaxNumIterations + stepCount].color = vec4(0.f, 1.f, 0.f, 1.f);
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
			t += (tCellBoundry + rayBump);
            level = min(level + 1, numLevels - 1);
			stepCount++;
		}
    }
    return bHit;
}
#else

bool hierarchicalTrace(in vec3 worldSpaceRO, in vec3 worldSpaceRD, inout float t, int rayID)
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
    screenSpaceT += 0.0001f;

    int level = numLevels - 1;
    int stepCount = 0;

	// clear debug buffer
	for (int i = 0; i < kMaxNumIterations; ++i)
	{
		debugTraces[i].minDepth = 0.f;
		debugTraces[i].stepDepth = 0.f;
		debugTraces[i].tDepth = 0.f;
		debugTraces[i].tCellBoundry = 0.f;
		debugTraces[i].t = 0.f;
		debugTraces[i].level = 0;
		debugTraces[i].iteration = 0;
		debugTraces[i].mipSize = vec2(0.f);
		debugTraces[i].cell = vec2(0.f);
		debugTraces[i].cellBoundry = vec4(0.f);
		debugTraces[i].pp = vec4(0.f);
		debugTraces[i].stepCount = 0;
		debugTraces[i].ro = vec4(ro, 1.f);
		debugTraces[i].rd = vec4(rd, 0.f);

		pointsAlongRay[rayID * kMaxNumIterations + i].position = vec4(0.f);
		pointsAlongRay[rayID * kMaxNumIterations + i].color = vec4(0.f);
	}

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

		// record debug tracing data
		debugTraces[i].minDepth = minDepth;
		debugTraces[i].stepDepth = pp.z;
		debugTraces[i].tDepth = tDepth;
		debugTraces[i].tCellBoundry = tCellBoundry;
		debugTraces[i].t = screenSpaceT;
		debugTraces[i].level = level;
		debugTraces[i].iteration = i;
		debugTraces[i].mipSize = mipSize;
		debugTraces[i].cell = floor(coord);
		debugTraces[i].cellBoundry = vec4(left, right, bottom, top);
		debugTraces[i].pp = vec4(pp, 1.f);
		debugTraces[i].stepCount = stepCount;

		// unproject pp back to world space 
		pointsAlongRay[rayID * kMaxNumIterations + stepCount].position = vec4(screenToWorld(vec3(pp * 2.f - 1.f), inverse(view), inverse(projection)), 1.f);
		pointsAlongRay[rayID * kMaxNumIterations + stepCount].color = vec4(1.f, 0.f, 0.f, 1.f);

		if (tDepth <= tCellBoundry)
		{
            // we find a good enough hit
			if (level <= 0)
            {
                if (tDepth > 0.f)
                {
					screenSpaceT += tDepth;
                    pp = ro + screenSpaceT * rd;
					pointsAlongRay[rayID * kMaxNumIterations + stepCount + 1].position = vec4(screenToWorld(vec3(pp * 2.f - 1.f), inverse(view), inverse(projection)), 1.f);
					pointsAlongRay[rayID * kMaxNumIterations + stepCount + 1].color = vec4(0.f, 1.f, 0.f, 1.f);
                }
                else 
                {
					pointsAlongRay[rayID * kMaxNumIterations + stepCount].color = vec4(0.f, 1.f, 0.f, 1.f);
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
			stepCount++;
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
#endif

// todo: improve this by applying spatiotemporal reuse
void calcAmbientOcclusionAndBentNormal(vec3 p, vec3 n, inout float ao, inout vec3 bentNormal) 
{
    int numOccludedRays = 0;
    for (int ray = 0; ray < numRays; ++ray)
    {
		float randomRotation = texture(blueNoiseTexture, debugCoord).r * PI * 2.f;
		vec3 rd = normalize(blueNoiseCosWeightedSampleHemisphere(n, BlueNoiseInDisk[ray], randomRotation));

        float t;
        bool bHit = hierarchicalTrace(p, rd, t, ray);
        if (bHit)
        {
            numOccludedRays++;
        }
	}
	ao = float(numOccludedRays) / float(numRays);
};

void main()
{
	float depth = texture(depthBuffer, debugCoord).r;
	vec3 normal = normalize(texture(normalBuffer, debugCoord).rgb * 2.f - 1.f);
    vec3 worldSpaceRO = screenToWorld(vec3(debugCoord, depth) * 2.f - 1.f, inverse(view), inverse(projection));

    float ao = 1.f;
    vec3 bentNormal;
    calcAmbientOcclusionAndBentNormal(worldSpaceRO, normal, ao, bentNormal);
}
