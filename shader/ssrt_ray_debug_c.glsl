#version 450 core
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
uniform vec2 debugCoord;
uniform sampler2D depthBuffer;
uniform sampler2D normalBuffer;
uniform sampler2D HiZ;
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

layout (std430) buffer DebugRayBuffer
{
    vec4 pointsAlongRay[];
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

bool hierarchicalTrace(in vec3 worldSpaceRO, in vec3 worldSpaceRD, inout float t)
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

    // slightly offset the ray origin in screen space to avoid self intersection
    t += 0.005;

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

		// unproject pp back to world space 
		pointsAlongRay[i] = vec4(0.f);
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
		pointsAlongRay[stepCount] = vec4(screenToWorld(vec3(pp * 2.f - 1.f), inverse(view), inverse(projection)), 1.f);

		if (tDepth <= tCellBoundry)
		{
            // we find a good enough hit
			if (level == 0)
            {
                t += tDepth;
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

void main()
{
	float depth = texture(depthBuffer, debugCoord).r;
	vec3 normal = normalize(texture(normalBuffer, debugCoord).rgb * 2.f - 1.f);
    vec3 worldSpaceRO = screenToWorld(vec3(debugCoord, depth) * 2.f - 1.f, inverse(view), inverse(projection));
    vec3 worldSpaceRD = normal;
    float t;
    bool bHit = hierarchicalTrace(worldSpaceRO, worldSpaceRD, t);
}
