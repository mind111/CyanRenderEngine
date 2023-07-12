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
void hizTrace(in vec3 worldSpaceRO, in vec3 worldSpaceRD, inout HitRecord hitRecord, mat4 viewMatrix, mat4 projectionMatrix)
{
    bool bHit = false;
	hitRecord.result = HitResult_Miss;
	mat4 view = viewMatrix;
	mat4 projection = projectionMatrix;

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
