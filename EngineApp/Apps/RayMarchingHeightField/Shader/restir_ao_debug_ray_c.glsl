#version 450 core

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

struct Ray
{
	vec4 ro;
	vec4 hit;
};

layout (std430, binding = 0) buffer DebugRayBuffer 
{
    Ray outRays[];
};

struct Trace
{
	vec4 uv;
	vec4 tCell;
	vec4 tHeight;
	vec4 tMarched;
	vec4 mipLevel;
	vec4 sceneHeight;
	vec4 rayHeight;
	vec4 rayEntryHeight;
};

layout (std430, binding = 1) buffer TraceBuffer 
{
    Trace outTraces[];
};

uniform vec2 u_debugPos;
uniform sampler2D u_sceneDepth;
uniform sampler2D u_sceneNormal;
uniform mat4 u_heightFieldTransformMatrix; 

bool intersectHeightFieldLocalSpaceAABB(vec3 ro, vec3 rd, inout float t0, inout float t1)
{
	bool bHit = false;

	// solve the intersection of view ray with the AABB of the height field
	vec3 pmin = vec3(-1.f, 0.f, -1.f);
	vec3 pmax = vec3( 1.f, 1.f,  1.f);
	t0 = -1.f / 0.f, t1 = 1.f / 0.f;

	float txmin = (rd.x == 0.f) ? -1e10 : (pmin.x - ro.x) / rd.x;
	float txmax = (rd.x == 0.f) ?  1e10 : (pmax.x - ro.x) / rd.x;
	float tx0 = min(txmin, txmax);
	float tx1 = max(txmin, txmax);
	t0 = max(tx0, t0);
	t1 = min(tx1, t1);

	float tymin = (rd.y == 0.f) ? -1e10 : (pmin.y - ro.y) / rd.y;
	float tymax = (rd.y == 0.f) ?  1e10 : (pmax.y - ro.y) / rd.y;
	float ty0 = min(tymin, tymax);
	float ty1 = max(tymin, tymax);
	t0 = max(ty0, t0);
	t1 = min(ty1, t1);

	float tzmin = (rd.z == 0.f) ? -1e10 : (pmin.z - ro.z) / rd.z;
	float tzmax = (rd.z == 0.f) ?  1e10 : (pmax.z - ro.z) / rd.z;
	float tz0 = min(tzmin, tzmax);
	float tz1 = max(tzmin, tzmax);
	t0 = max(tz0, t0);
	t1 = min(tz1, t1);
	t0 = max(t0, 0.f);

	if (t0 < t1)
	{
		bHit = true;
	}
	return bHit;
}

struct HitRecord
{
	bool bHit;
	vec3 worldSpaceHit;
	int numMarchedSteps;
};

uniform sampler2D u_heightFieldQuadTreeTexture;
uniform int u_numMipLevels;

vec3 transformPositionLocalSpaceToTextureSpace(vec3 p)
{
	vec3 pp = p - vec3(-1.f, 0.f, 1.f);
	pp.xz /= 2.f;
	pp.z *= -1.f;
	return pp;
}

vec3 transformDirectionLocalSpaceToTextureSpace(vec3 d)
{
	vec3 dd = d;
	dd.xz /= 2.f;
	dd.z *= -1.f;
	return dd;
}

// assumes that both p and d are in texture uv space 
float calcIntersectionWithQuadTreeCell(vec2 p, vec2 d, float mip0TexelSize, int mipLevel)
{

	float t = 1.f / 0.f;
	float cellSize = mip0TexelSize * pow(2, mipLevel);
	vec2 cellBottom = floor(p / cellSize) * cellSize;
	float left = cellBottom.x;
	float right = cellBottom.x + cellSize;
	float bottom = cellBottom.y;
	float top = cellBottom.y + cellSize;

	float tLeft = (left - p.x) / max(d.x, 1e-6);
	float tRight = (right - p.x) / max(d.x, 1e-6);
	float tTop = (top - p.y) / max(d.y, 1e-6);
	float tBottom = (bottom - p.y) / max(d.y, 1e-6);

	t = tLeft > 0.f ? min(tLeft, t) : t;
	t = tRight > 0.f ? min(tRight, t) : t;
	t = tBottom > 0.f ? min(tBottom, t) : t;
	t = tTop > 0.f ? min(tTop, t) : t;

	return t;
}

HitRecord hierarchicalRayMarching(in vec3 localSpaceRo, in vec3 localSpaceRd)
{
	HitRecord outHitRecord;
	outHitRecord.bHit = false;
	outHitRecord.worldSpaceHit = vec3(0.f);
	outHitRecord.numMarchedSteps = 0;

	float mip0TexelSize = 1.f / textureSize(u_heightFieldQuadTreeTexture, 0).x;
	float t0, t1;
	if (intersectHeightFieldLocalSpaceAABB(localSpaceRo, localSpaceRd, t0, t1))
	{
		vec3 rayEntry = localSpaceRo + t0 * localSpaceRd;
		vec3 rayExit = localSpaceRo + t1 * localSpaceRd;

		const int maxNumTraceSteps = 128;

		// convert ro, and rd to texture uv space
		vec3 ro = transformPositionLocalSpaceToTextureSpace(rayEntry);
		vec3 re = transformPositionLocalSpaceToTextureSpace(rayExit);
		vec3 rd = transformDirectionLocalSpaceToTextureSpace(localSpaceRd);

		/**
		 * Parameterize the ray such that marched distance t is always within range [0.f, 1.f], when t = 0.f, 
		 * rayHeight is either 0 or 1, when t = 1, rayHeight is also either 0 or 1
		 */ 
		float t = 0.f, tMax = 0.f;
		if (rd.y > 0.f)
		{
			rd /= abs(rd.y); 
			float tStart = ro.y;
			tMax = re.y - tStart;
		}
		else if (rd.y < 0.f) 
		{
			rd /= abs(rd.y);
			float tStart = 1.f - ro.y;
			tMax = (1.f - re.y) - tStart;
		}
		else
		{
			tMax = t1 - t0;
		}

		vec3 p = ro + t * rd;

		int startLevel = rd.y >= 0.f ? 1 : (u_numMipLevels - 1);
		int currentLevel = startLevel;
		for (int i = 0; i < maxNumTraceSteps; ++i)
		{
            Trace trace;
			trace.mipLevel = vec4(currentLevel);

			// stop marching if we find a good enough hit
			if (currentLevel < 0)
			{
				outHitRecord.bHit = true;

				/**
				 * when a hit is found, the ray will always stop at the texel edge, need to 
				 * snap it to texel center to get correct height, and march a small step to
				 * reach the height plane to find xz 
				 */
				vec3 textureSpaceHit = p;
				textureSpaceHit.xz = (floor(textureSpaceHit.xz / mip0TexelSize) + .5f) * mip0TexelSize;
				ivec2 mip0Size = textureSize(u_heightFieldQuadTreeTexture, 0).xy;
				ivec2 texCoordi = ivec2(floor(textureSpaceHit.xz * mip0Size));
				float sceneHeightAtHit = texelFetch(u_heightFieldQuadTreeTexture, texCoordi, 0).r;
				textureSpaceHit.y = sceneHeightAtHit;

				// backoff the ray to cell boundary
				const float kOffset = 0.0001f;
				p += -kOffset / length(rd) * rd;

				float tHeight = p.y - sceneHeightAtHit;
				// keep marching till the ray hit height plane
				if (tHeight > 0.f)
				{
					textureSpaceHit = p + tHeight * rd;
				}
				// the ray hit the side
				else
				{
					textureSpaceHit = p;
				}

				vec3 localSpaceHitPosition;
				localSpaceHitPosition.xz = vec2(textureSpaceHit.x, -textureSpaceHit.z) * 2.f + vec2(-1.f, 1.f);
				localSpaceHitPosition.y = textureSpaceHit.y;

				vec3 worldSpaceHitPosition = (u_heightFieldTransformMatrix * vec4(localSpaceHitPosition, 1.f)).xyz;
				outHitRecord.worldSpaceHit = worldSpaceHitPosition;
				outHitRecord.numMarchedSteps = i;
				break;
			}

			// stop marching when the ray is exiting the height field AABB, no hit is found
			if (t >= tMax) break;

			p = ro + t * rd;

			float rayHeight = p.y;
			float tCell = calcIntersectionWithQuadTreeCell(p.xz, rd.xz, mip0TexelSize, currentLevel);
			ivec2 mipLevelSize = textureSize(u_heightFieldQuadTreeTexture, currentLevel).xy;
			ivec2 texCoordi = ivec2(floor(p.xz * mipLevelSize));
			float sceneHeight = texelFetch(u_heightFieldQuadTreeTexture, texCoordi, currentLevel).r;
			float tHeight = (sceneHeight - rayHeight) / rd.y;
			tHeight = tHeight < 0.f ? 1.f / 0.f : tHeight;

			trace.uv = vec4(p.xz, 0.f, 1.f);
			trace.tCell = vec4(tCell);
			trace.tHeight = vec4(tHeight);
			trace.tMarched = vec4(t);
			trace.sceneHeight = vec4(sceneHeight);
			trace.rayHeight = vec4(rayHeight);
			trace.rayEntryHeight = vec4(localSpaceRo.y);
			outTraces[i] = trace;

			if (rayHeight > sceneHeight && tCell <= tHeight)
			{
				// go up a level to perform coarser trace 
				t += tCell;
				currentLevel += 1;
				currentLevel = min(currentLevel, u_numMipLevels);

				/**
				 * This offset is important as it helps the calculation of intersection with quadtree 
				 * cell boundary as well as using correct tex coordinate to sample the height quadtree.
				 * It avoids having the ray getting stuck at quadtree cell boundary.
				 * Bumping the ray with a small offset here works and it's a easy way to work around the
				 * issues brought by having the ray stuck at the quadtree cell boundry, but need 
				 * some trial-and-error to find the right value to use or else artifacts may appear.
				 */
				// offset the ray slightly to avoid it stopping on the quadtree cell boundary
				const float kOffset = 0.0001f; // offset constant in uv space
				// take the length of parameterized rd into consideration, 0.0001 in uv space can be a tiny offset for rd
				// if length(rd) is large
				t += kOffset / length(rd);
			}
			else
			{
				// go down a level to perform more detailed trace
				currentLevel -= 1;
			}
			outHitRecord.numMarchedSteps += 1;
		}
	}

	return outHitRecord;
}

uniform mat4 u_viewMatrix;
uniform mat4 u_projectionMatrix;

vec3 transformPositionScreenToWorld(vec3 pp)
{
    vec4 p = inverse(u_projectionMatrix) * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = inverse(u_viewMatrix) * p;
    return p.xyz;
}

void main()
{
	vec2 heightFieldSize = textureSize(u_heightFieldQuadTreeTexture, 0).xy;
	vec2 heightFieldSpaceUV = u_debugPos * .5f + .5f;
	heightFieldSpaceUV.y = 1.f - heightFieldSpaceUV.y;
	heightFieldSpaceUV = (floor(heightFieldSpaceUV * heightFieldSize) + .5f) / heightFieldSize;
	ivec2 texCoordi = ivec2(floor(heightFieldSpaceUV * heightFieldSize));
	float height = texelFetch(u_heightFieldQuadTreeTexture, texCoordi, 0).r;

	heightFieldSpaceUV.y = 1.f - heightFieldSpaceUV.y;
	heightFieldSpaceUV = heightFieldSpaceUV * 2.f - 1.f;

	vec3 localSpacePos = vec3(heightFieldSpaceUV.x, height, heightFieldSpaceUV.y);
	vec3 worldSpacePos = (u_heightFieldTransformMatrix * vec4(localSpacePos, 1.f)).xyz;
    vec3 n = vec3(0.f, 1.f, 0.f);
    vec3 rd = n;
    vec3 worldSpaceRo = worldSpacePos + n * .001f;
    vec3 localSpaceRo = (inverse(u_heightFieldTransformMatrix) * vec4(worldSpaceRo, 1.f)).xyz;
    vec3 localSpaceRd = (inverse(u_heightFieldTransformMatrix) * vec4(rd, 0.f)).xyz;

    HitRecord hitRecord = hierarchicalRayMarching(localSpaceRo, localSpaceRd);

    if (hitRecord.bHit)
    {
        Ray ray;
        ray.ro = vec4(localSpaceRo, 1.f);
        ray.hit = vec4(hitRecord.worldSpaceHit, 1.f);
        outRays[0] = ray;
    }
}
