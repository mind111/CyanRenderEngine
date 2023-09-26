#version 450 core

#define SNOW_LAYER 0
#define BASE_LAYER 1
#define INVALID_LAYER -1

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

struct Trace
{
	ivec4 stepID;
	vec4 t;
	vec4 textureSpacePos;
	ivec4 mipLevel;
	vec4 tQuadTreeCell;
	vec4 cellBound;
	ivec4 mipLevelSize;
	ivec4 texCoordi;
	vec4 sceneHeight;
	vec4 rayHeight;
	vec4 tHeight;
};

layout (std430, binding = 0) buffer TraceBuffer 
{
    Trace outTraces[];
};

uniform vec3 u_debugRo;
uniform vec3 u_debugRd;
uniform mat4 u_heightFieldTransformMat; 
uniform int u_maxNumSteps;

struct HitRecord
{
	bool bHit;
	vec2 textureSpaceHit;
	vec3 worldSpaceHit;
	int numMarchedSteps;
	int layer;
	float cloudOpacity;
};

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

uniform sampler2D u_heightFieldQuadTreeTexture;
uniform int u_numMipLevels;

// assumes that both p and d are in texture uv space 
float calcIntersectionWithQuadTreeCell(vec2 p, vec2 d, float mip0TexelSize, int mipLevel)
{
	// offset the starting position slightly
	const float kOffset = 0.0005f;
	p += d * kOffset;

	float t = 1.f / 0.f;
	float cellSize = mip0TexelSize * pow(2, mipLevel);
	vec2 cellBottom = floor(p / cellSize) * cellSize;
	float left = cellBottom.x;
	float right = cellBottom.x + cellSize;
	float bottom = cellBottom.y;
	float top = cellBottom.y + cellSize;

	float tLeft = (left - p.x) / d.x;
	float tRight = (right - p.x) / d.x;
	float tTop = (top - p.y) / d.y;
	float tBottom = (bottom - p.y) / d.y;

	t = tLeft > 0.f ? min(tLeft, t) : t;
	t = tRight > 0.f ? min(tRight, t) : t;
	t = tBottom > 0.f ? min(tBottom, t) : t;
	t = tTop > 0.f ? min(tTop, t) : t;

	return (t + kOffset);
}

// todo: debug this trace procedure
HitRecord hierarchicalRayMarching(in vec3 localSpaceRo, in vec3 localSpaceRd)
{
	HitRecord outHitRecord;
	outHitRecord.bHit = false;
	outHitRecord.textureSpaceHit = vec2(0.f);
	outHitRecord.worldSpaceHit = vec3(0.f);
	outHitRecord.numMarchedSteps = 0;
	outHitRecord.layer = INVALID_LAYER;
	outHitRecord.cloudOpacity = 0.f;

	float mip0TexelSize = 1.f / textureSize(u_heightFieldQuadTreeTexture, 0).x;
	float t0, t1;

	#if 0
	{
		vec3 ro = localSpaceRo, rd = localSpaceRd;
		// solve the intersection of view ray with the AABB of the height field
		vec3 pmin = vec3(-1.f, 0.f, -1.f);
		vec3 pmax = vec3( 1.f, 1.f,  1.f);
		t0 = -1.f / 0.f, t1 = 1.f / 0.f;
		float txmin = (pmin.x - ro.x) / rd.x;
		float txmax = (pmax.x - ro.x) / rd.x;
		float tx0 = min(txmin, txmax);
		float tx1 = max(txmin, txmax);
		t0 = max(tx0, t0);
		t1 = min(tx1, t1);

		float tymin = (pmin.y - ro.y) / rd.y;
		float tymax = (pmax.y - ro.y) / rd.y;
		float ty0 = min(tymin, tymax);
		float ty1 = max(tymin, tymax);
		t0 = max(ty0, t0);
		t1 = min(ty1, t1);

		// float tzmin = rd.z == 0.f ? -1e10 : (pmin.z - ro.z) / rd.z;
		// float tzmax = rd.z == 0.f ?  1e10 : (pmax.z - ro.z) / rd.z;
		float tzmin = (pmin.z - ro.z) / rd.z;
		float tzmax = (pmax.z - ro.z) / rd.z;
		float tz0 = min(tzmin, tzmax);
		float tz1 = max(tzmin, tzmax);
		t0 = max(tz0, t0);
		t1 = min(tz1, t1);
		t0 = max(t0, 0.f);
	}
	#endif

	if (intersectHeightFieldLocalSpaceAABB(localSpaceRo, localSpaceRd, t0, t1))
	{
		vec3 rayEntry = localSpaceRo + t0 * localSpaceRd;
		vec3 rayExit = localSpaceRo + t1 * localSpaceRd;

		const int maxNumTraceSteps = 64;

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

		vec3 p = ro + t * rd;

		int startLevel = rd.y >= 0.f ? 1 : (u_numMipLevels - 1);
		int currentLevel = startLevel;
		for (int i = 0; i < maxNumTraceSteps; ++i)
		{
			if (currentLevel <= 0)
			{
				outHitRecord.bHit = true;
				outHitRecord.textureSpaceHit = p.xz;

				vec3 localSpaceHitPosition = p.xyz * 2.f + vec3(-1.f, 0.f, 1.f);
				vec3 worldSpaceHitPosition = (u_heightFieldTransformMat * vec4(localSpaceHitPosition, 1.f)).xyz;
				outHitRecord.worldSpaceHit = worldSpaceHitPosition;
				outHitRecord.numMarchedSteps = i;
				outHitRecord.layer = SNOW_LAYER;  
				outHitRecord.cloudOpacity = 0.f;
				break;
			}

			if (t >= tMax) break;

			p = ro + t * rd;
			float rayHeight = p.y;
			float tCell = calcIntersectionWithQuadTreeCell(p.xz, rd.xz, mip0TexelSize, currentLevel);

			float cellSize = mip0TexelSize * pow(2, currentLevel);
			vec2 cellBottom = floor(p.xz / cellSize) * cellSize;
			float left = cellBottom.x;
			float right = cellBottom.x + cellSize;
			float bottom = cellBottom.y;
			float top = cellBottom.y + cellSize;

			ivec2 mipLevelSize = textureSize(u_heightFieldQuadTreeTexture, currentLevel).xy;
			ivec2 texCoordi = ivec2(floor(p.xz * mipLevelSize));
			float sceneHeight = texelFetch(u_heightFieldQuadTreeTexture, texCoordi, currentLevel).r;
			float tHeight = rayHeight - sceneHeight;
			if (tCell <= tHeight)
			{
				// go up a level to perform coarser trace 
				t += tCell;
				currentLevel += 1;
				currentLevel = min(currentLevel, u_numMipLevels);
			}
			else
			{
				// go down a level to perform more detailed trace
				currentLevel -= 1;
				currentLevel = max(currentLevel, 0);
			}
			outHitRecord.numMarchedSteps += 1;

           Trace trace;
		   trace.stepID = ivec4(i, i, i, 1);
		   trace.t = vec4(t);
		   trace.textureSpacePos = vec4(p.xz, 0.f, 0.f);
		   trace.mipLevel = ivec4(currentLevel);
		   trace.tQuadTreeCell = vec4(tCell);
		   trace.cellBound.x = left;
		   trace.cellBound.y = right;
		   trace.cellBound.z = bottom;
		   trace.cellBound.w = top;
		   trace.rayHeight = vec4(rayHeight);
		   trace.mipLevelSize = ivec4(mipLevelSize, mipLevelSize); 
		   trace.texCoordi = ivec4(texCoordi, texCoordi);
		   trace.sceneHeight = vec4(sceneHeight);
		   trace.tHeight = vec4(tHeight);

           outTraces[i] = trace;
		}
	}

	return outHitRecord;
}


void main()
{
    // transform ro, rd to height field's local space
    vec3 localSpaceRo = (inverse(u_heightFieldTransformMat) * vec4(u_debugRo, 1.f)).xyz;
    vec3 localSpaceRd = (inverse(u_heightFieldTransformMat) * vec4(u_debugRd, 0.f)).xyz;
	HitRecord hr = hierarchicalRayMarching(localSpaceRo, localSpaceRd);
}