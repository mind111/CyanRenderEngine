#version 450 core

#define PI 3.1415926
#define TEMPORAL_REPROJECTION 1

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outVBAO;

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
uniform sampler2D u_prevFrameVBAOTex;

// voxel stuffs
uniform vec3 u_voxelGridAABBMin;
uniform vec3 u_voxelGridAABBMax;
uniform vec3 u_voxelGridCenter;
uniform vec3 u_voxelGridExtent;
uniform vec3 u_voxelGridResolution;
// uniform float u_voxelSize;
uniform vec3 u_voxelSize;
uniform sampler3D u_voxelGridOpacityTex;

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

bool intersectAABB(vec3 ro, vec3 rd, inout float t0, inout float t1, vec3 pmin, vec3 pmax)
{
	bool bHit = false;

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
	ivec3 voxelCoord;
	vec3 worldSpacePosition;
};

ivec3 worldSpacePositionToVoxelCoord(vec3 worldSpacePosition, int mipLevel)
{
    vec3 p = u_voxelGridCenter + vec3(-u_voxelGridExtent.x, -u_voxelGridExtent.y, u_voxelGridExtent.z);
    vec3 uv = (worldSpacePosition - p) / (u_voxelGridExtent * 2.f);
    uv.z *= -1.f;
	ivec3 mipResolution = ivec3(u_voxelGridResolution) / int(pow(2, mipLevel));
    ivec3 coord = ivec3(floor(uv * mipResolution));
	return coord;
}

HitRecord rayMarchingVoxelGrid(vec3 ro, vec3 rd)
{
	HitRecord outHitRecord;
	outHitRecord.bHit = false;

	float tEnter, tExit;
	if (intersectAABB(ro, rd, tEnter, tExit, u_voxelGridAABBMin, u_voxelGridAABBMax))
	{
		vec3 rayStart = ro + rd * tEnter;
		ivec3 voxelCoord = worldSpacePositionToVoxelCoord(rayStart, 0);

		int stepX = int(sign(rd.x));
		int stepY = int(sign(rd.y));
		int stepZ = int(sign(rd.z));

		float tDeltaX = u_voxelSize.x / abs(rd.x);
		float tDeltaY = u_voxelSize.y / abs(rd.y);
		float tDeltaZ = u_voxelSize.z / abs(rd.z);

		vec3 voxelGridOrigin = u_voxelGridCenter + vec3(-u_voxelGridExtent.x, -u_voxelGridExtent.y, u_voxelGridExtent.z);

		float tMaxX, tMaxY, tMaxZ;
		float x0 = voxelGridOrigin.x + voxelCoord.x * u_voxelSize.x;
		float x1 = voxelGridOrigin.x + (voxelCoord.x + 1) * u_voxelSize.x;
		tMaxX = max((x0 - ro.x) / rd.x, (x1 - ro.x) / rd.x);

		float y0 = voxelGridOrigin.y + voxelCoord.y * u_voxelSize.y;
		float y1 = voxelGridOrigin.y + (voxelCoord.y + 1) * u_voxelSize.y;
		tMaxY = max((y0 - ro.y) / rd.y, (y1 - ro.y) / rd.y);

		float z0 = voxelGridOrigin.z - voxelCoord.z * u_voxelSize.z;
		float z1 = voxelGridOrigin.z - (voxelCoord.z + 1) * u_voxelSize.z;
		tMaxZ = max((z0 - ro.z) / rd.z, (z1 - ro.z) / rd.z);

		float t = tEnter;
		while (t < tExit)
		{
			// if opaque, stop marching
			float opacity = texelFetch(u_voxelGridOpacityTex, voxelCoord, 0).r;
			if (opacity > 0.f)
			{
				outHitRecord.bHit = true;
				outHitRecord.voxelCoord = voxelCoord;
				outHitRecord.worldSpacePosition = ro + t * rd;
				break;
			}

			if (tMaxX <= tMaxY && tMaxX <= tMaxZ)
			{
				t = tMaxX;
				tMaxX += tDeltaX;
				voxelCoord += ivec3(stepX, 0, 0);
			}
			else if (tMaxY <= tMaxX && tMaxY <= tMaxZ)
			{
				t = tMaxY;
				tMaxY += tDeltaY;
				voxelCoord += ivec3(0, stepY, 0);
			}
			else 
			{
				t = tMaxZ;
				tMaxZ += tDeltaZ;
				voxelCoord += ivec3(0, 0, -stepZ);
			}
		}
	}
	return outHitRecord;
}

uniform int u_maxNumIterations;
uniform int u_numMipLevels;

HitRecord hierarchicalRayMarchingVoxelGrid(vec3 ro, vec3 rd)
{
	HitRecord outHitRecord;
	outHitRecord.bHit = false;

	float tEnter, tExit;
	if (intersectAABB(ro, rd, tEnter, tExit, u_voxelGridAABBMin, u_voxelGridAABBMax))
	{
		vec3 voxelGridOrigin = u_voxelGridCenter + vec3(-u_voxelGridExtent.x, -u_voxelGridExtent.y, u_voxelGridExtent.z);
		vec3 p = ro + rd * tEnter;
		int startLevel = 0;
		int currLevel = 0;
		ivec3 voxelCoord = worldSpacePositionToVoxelCoord(p, currLevel);
		float t = 0.f;
		for (int i = 0; i < u_maxNumIterations; ++i)
		{
			if (currLevel < 0)
			{
				outHitRecord.bHit = true;
				outHitRecord.voxelCoord = voxelCoord;
				outHitRecord.worldSpacePosition = p;
				break;
			}

			if (currLevel >= u_numMipLevels || t >= (tExit - tEnter))
			{
				break;
			}

			// offset the ray in world space slightly when sampling the volume texture to avoid the
			// ray getting stuck at octree cell boundaries
			voxelCoord = worldSpacePositionToVoxelCoord(p + rd * 0.001, currLevel);

			float opacity = texelFetch(u_voxelGridOpacityTex, voxelCoord, currLevel).r;
			if (opacity > 0.f)
			{
				currLevel -= 1;
			}
			else
			{
				// todo: this calculation can be improved 
				vec3 mipVoxelSize = u_voxelSize * pow(2, currLevel);
				#if 0
				float tx, ty, tz;
				float x0 = voxelGridOrigin.x + voxelCoord.x * mipVoxelSize.x;
				float x1 = voxelGridOrigin.x + (voxelCoord.x + 1) * mipVoxelSize.x;
				tx = max((x0 - p.x) / rd.x, (x1 - p.x) / rd.x);

				float y0 = voxelGridOrigin.y + voxelCoord.y * mipVoxelSize.y;
				float y1 = voxelGridOrigin.y + (voxelCoord.y + 1) * mipVoxelSize.y;
				ty = max((y0 - p.y) / rd.y, (y1 - p.y) / rd.y);

				float z0 = voxelGridOrigin.z - voxelCoord.z * mipVoxelSize.z;
				float z1 = voxelGridOrigin.z - (voxelCoord.z + 1) * mipVoxelSize.z;
				tz = max((z0 - p.z) / rd.z, (z1 - p.z) / rd.z);

				if (tx <= ty && tx <= tz)
				{
					p += tx * rd;
					t += tx;
				}
				else if (ty <= tx && ty <= tz)
				{
					p += ty * rd;
					t += ty;
				}
				else 
				{
					p += tz * rd;
					t += tz;
				}
				#else
				// this is a slightly faster version of the code above
				vec3 dt0, dt1, dt;
				vec3 q0 = voxelGridOrigin + voxelCoord * vec3(mipVoxelSize.xy, -mipVoxelSize.z);
				vec3 q1 = voxelGridOrigin + (voxelCoord + 1) * vec3(mipVoxelSize.xy, -mipVoxelSize.z);
				dt0 = (q0 - p) / rd;
				dt1 = (q1 - p) / rd;
				dt = max(dt0, dt1);
				float tmin = min(min(dt.x, dt.y), dt.z);
				p += tmin * rd;
				t += tmin;
				#endif

				currLevel += 1;
			}
		}
	}
	return outHitRecord;
}

void offsetRayOneVoxel(inout vec3 ro, vec3 d)
{
	float startingOffset = 0.f;
	ivec3 startingVoxelCoord = worldSpacePositionToVoxelCoord(ro, 0);
	vec3 voxelGridOrigin = u_voxelGridCenter + vec3(-u_voxelGridExtent.x, -u_voxelGridExtent.y, u_voxelGridExtent.z);
	float tMaxX, tMaxY, tMaxZ;
	float x0 = voxelGridOrigin.x + startingVoxelCoord.x * u_voxelSize.x;
	float x1 = voxelGridOrigin.x + (startingVoxelCoord.x + 1) * u_voxelSize.x;
	tMaxX = max((x0 - ro.x) / d.x, (x1 - ro.x) / d.x);

	float y0 = voxelGridOrigin.y + startingVoxelCoord.y * u_voxelSize.y;
	float y1 = voxelGridOrigin.y + (startingVoxelCoord.y + 1) * u_voxelSize.y;
	tMaxY = max((y0 - ro.y) / d.y, (y1 - ro.y) / d.y);

	float z0 = voxelGridOrigin.z - startingVoxelCoord.z * u_voxelSize.z;
	float z1 = voxelGridOrigin.z - (startingVoxelCoord.z + 1) * u_voxelSize.z;
	tMaxZ = max((z0 - ro.z) / d.z, (z1 - ro.z) / d.z);

	if (tMaxX <= tMaxY && tMaxX <= tMaxZ)
	{
		startingOffset = tMaxX;
	}
	else if (tMaxY <= tMaxX && tMaxY <= tMaxZ)
	{
		startingOffset = tMaxY;
	}
	else 
	{
		startingOffset = tMaxZ;
	}
	ro += d * startingOffset;
}

// todo: temporal reprojection
void main()
{
    seed = viewParameters.frameCount;
    flat_idx = int(floor(gl_FragCoord.y) * viewParameters.renderResolution.x + floor(gl_FragCoord.x));

	float depth = texture(u_sceneDepthTex, psIn.texCoord0).r;
	if (depth == 1.f) discard;

	vec3 n = normalize(texture(u_sceneNormalTex, psIn.texCoord0).rgb * 2.f - 1.f);

	mat4 view = viewParameters.viewMatrix, projection = viewParameters.projectionMatrix;
	vec3 worldSpacePosition = screenToWorld(vec3(psIn.texCoord0, depth) * 2.f - 1.f, inverse(view), inverse(projection));

	// offset 1 meter to avoid self intersection
	vec3 ro = worldSpacePosition + n * .03f;

#if !TEMPORAL_REPROJECTION
	const int kNumSamples = 8;

	float ao = 0.f;
	for (int i = 0; i < kNumSamples; ++i)
	{
		vec3 rd = uniformSampleHemisphere(n);
		// HitRecord hitRecord = rayMarchingVoxelGrid(ro, rd);
		HitRecord hitRecord = hierarchicalRayMarchingVoxelGrid(ro, rd);
		if (!hitRecord.bHit)
		{
			ao += 1.f;
		}
	}
	ao /= kNumSamples;
	outVBAO = vec3(ao);
#else
	vec3 rd = uniformSampleHemisphere(n);
	// HitRecord hitRecord = rayMarchingVoxelGrid(ro, rd);
	HitRecord hitRecord = hierarchicalRayMarchingVoxelGrid(ro, rd);

	float visibility = 1.f;
	if (hitRecord.bHit)
	{
		visibility = 0.f;
	}

	outVBAO = vec3(visibility);

    if (viewParameters.frameCount > 0)
    {
        mat4 prevFrameViewMatrix = viewParameters.prevFrameViewMatrix;
        mat4 prevFrameProjectionMatrix = viewParameters.prevFrameProjectionMatrix;

        vec3 prevViewSpacePos = (prevFrameViewMatrix * vec4(worldSpacePosition, 1.f)).xyz;
		vec4 prevNDCPos = prevFrameProjectionMatrix * vec4(prevViewSpacePos, 1.f);
        prevNDCPos /= prevNDCPos.w;
        prevNDCPos.xyz = prevNDCPos.xyz * .5f + .5f;

        if (prevNDCPos.x <= 1.f && prevNDCPos.x >= 0.f && prevNDCPos.y <= 1.f && prevNDCPos.y >= 0.f)
        {
			float prevFrameDeviceZ = texture(u_prevFrameSceneDepthTex, prevNDCPos.xy).r;
            vec3 cachedPrevFrameViewSpacePos = (prevFrameViewMatrix * vec4(screenToWorld(vec3(prevNDCPos.xy, prevFrameDeviceZ) * 2.f - 1.f, inverse(prevFrameViewMatrix), inverse(prevFrameProjectionMatrix)), 1.f)).xyz;
            float relativeDepthDelta = abs(cachedPrevFrameViewSpacePos.z - prevViewSpacePos.z) / -cachedPrevFrameViewSpacePos.z;

			if (relativeDepthDelta < 0.1f)
			{
				float AOHistory = texture(u_prevFrameVBAOTex, prevNDCPos.xy).r;
				outVBAO = vec3(AOHistory * .9f + visibility * .1f);
			}
		}
	}
#endif
}
