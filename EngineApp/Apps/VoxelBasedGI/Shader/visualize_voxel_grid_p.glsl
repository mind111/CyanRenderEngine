#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

layout(location = 0) out vec3 outColor;

uniform sampler3D u_voxelGridOpacityTex;
uniform sampler3D u_voxelGridTargetTex;

uniform vec3 u_voxelGridAABBMin;
uniform vec3 u_voxelGridAABBMax;
uniform vec3 u_voxelGridCenter;
uniform vec3 u_voxelGridExtent;
uniform vec3 u_voxelGridResolution;
uniform vec3 u_voxelSize;
uniform int u_mipLevel;

struct ViewParameters
{
	uvec2 renderResolution;
	float aspectRatio;
	mat4 viewMatrix;
	mat4 projectionMatrix;
	vec3 cameraPosition;
	vec3 cameraRight;
	vec3 cameraForward;
	vec3 cameraUp;
	int frameCount;
	float elapsedTime;
	float deltaTime;
};

uniform ViewParameters viewParameters;
uniform vec2 u_renderResolution;

uniform float u_cameraFov;
uniform float u_cameraNearClippingPlane;

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

ivec3 worldSpacePositionToVoxelCoord(vec3 worldSpacePosition, int mipLevel)
{
    vec3 p = u_voxelGridCenter + vec3(-u_voxelGridExtent.x, -u_voxelGridExtent.y, u_voxelGridExtent.z);
    vec3 uv = (worldSpacePosition - p) / (u_voxelGridExtent * 2.f);
    uv.z *= -1.f;
	ivec3 mipResolution = ivec3(u_voxelGridResolution) / int(pow(2, mipLevel));
    ivec3 coord = ivec3(floor(uv * mipResolution));
	return coord;
}

struct HitRecord
{
	bool bHit;
	ivec3 voxelCoord;
	vec3 worldSpacePosition;
	int mipLevel;
};

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
		int currLevel = u_mipLevel;
		ivec3 voxelCoord = worldSpacePositionToVoxelCoord(p, currLevel);
		float t = 0.f;
		for (int i = 0; i < u_maxNumIterations; ++i)
		{
			if (currLevel < u_mipLevel)
			{
				outHitRecord.bHit = true;
				outHitRecord.voxelCoord = voxelCoord;
				outHitRecord.worldSpacePosition = p;
				outHitRecord.mipLevel = u_mipLevel;
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

				currLevel += 1;
			}
		}
	}
	return outHitRecord;
}

void main()
{
	// calculate rd
	vec2 uv = gl_FragCoord.xy / u_renderResolution;
	uv = uv * 2.f - 1.f;
	vec3 cy = u_cameraNearClippingPlane * tan(radians(u_cameraFov * .5f)) * viewParameters.cameraUp;
	float aspect = u_renderResolution.x / u_renderResolution.y;
	vec3 cx = u_cameraNearClippingPlane * tan(radians(u_cameraFov * .5f)) * aspect * viewParameters.cameraRight;
	vec3 rd = normalize(uv.x * cx  + uv.y * cy + u_cameraNearClippingPlane * viewParameters.cameraForward);

	float numMarchedSteps = 0.f;
	// solve for ro, the position where the ray enters into the voxel grid
	vec3 cameraPosition = viewParameters.cameraPosition;
	float tEnter, tExit;
	if (intersectAABB(cameraPosition, rd, tEnter, tExit, u_voxelGridAABBMin, u_voxelGridAABBMax))
	{
		vec3 ro = cameraPosition + rd * tEnter;
		HitRecord hitRecord = hierarchicalRayMarchingVoxelGrid(ro, rd);
		if (hitRecord.bHit)
		{
			vec3 mipResolution = u_voxelGridResolution / pow(2, hitRecord.mipLevel);
			vec3 uv = hitRecord.voxelCoord / mipResolution;
			outColor = vec3(uv);
			outColor = texelFetch(u_voxelGridTargetTex, hitRecord.voxelCoord, hitRecord.mipLevel).rgb;
		}
	}
}
