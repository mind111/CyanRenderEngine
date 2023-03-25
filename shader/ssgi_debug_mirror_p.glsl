#version 450 core

uniform sampler2D sceneDepth;
uniform sampler2D sceneNormal;
uniform sampler2D HiZ;
uniform sampler2D directDiffuseBuffer;
uniform int kMaxNumIterations;
uniform int numLevels;

#define PI 3.1415926

in VSOutput
{	
	vec2 texCoord0;
} psIn;

layout(std430) buffer ViewBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
};

out vec3 outColor;

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
            float rayBump = 0.1f * length(rd.xy) * 1.f / min(textureSize(HiZ, 0).x, textureSize(HiZ, 0).y);
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

void main()
{
	float depth = texture(sceneDepth, psIn.texCoord0).r;
	if (depth > 0.9999f)
	{
		discard;
	}

	vec3 worldSpacePosition = screenToWorld(vec3(psIn.texCoord0, depth) * 2.f - 1.f, inverse(view), inverse(projection));
    vec3 worldSpaceNormal = normalize(texture(sceneNormal, psIn.texCoord0).rgb * 2.f - 1.f);

	vec3 viewDirection = normalize(-(view * vec4(worldSpacePosition, 1.f)).xyz);
	vec3 worldSpaceViewDirection = (inverse(view) * vec4(viewDirection, 0.f)).xyz;

    // get the mirror reflection direction
    vec3 rd = -reflect(worldSpaceViewDirection, worldSpaceNormal);
    float t;
    bool bHit = hierarchicalTrace(worldSpacePosition, rd, t);
    if (bHit)
    {
        vec3 hitPosition = worldSpacePosition + t * rd;
        vec4 clipSpaceHit = projection * view * vec4(hitPosition, 1.f);
        clipSpaceHit /= clipSpaceHit.w;
        vec2 uv = clipSpaceHit.xy * .5f + .5f;
        vec3 n = normalize(texture(sceneNormal, uv).rgb * 2.f - 1.f);
		vec3 radiance = texture(directDiffuseBuffer, uv).rgb;
		outColor = radiance;
    }
}
