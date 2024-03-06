#version 450 core

#define PI 3.1415926

layout (local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(binding = 0, rgba16f) uniform image3D u_voxelGridIrradiance;

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
uniform ivec3 u_imageSize;
uniform sampler3D u_voxelGridNormalTex;
uniform sampler3D u_voxelGridOpacityTex;
uniform vec3 u_voxelGridCenter;
uniform vec3 u_voxelGridExtent;
uniform vec3 u_voxelGridResolution;
uniform vec3 u_voxelGridAABBMin;
uniform vec3 u_voxelGridAABBMax;
uniform vec3 u_voxelSize;
uniform samplerCube u_skyCubemapTex;

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
				vec3 mipVoxelSize = u_voxelSize * pow(2, currLevel);
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

vec3 offsetRayStart(vec3 voxelCenter, vec3 voxelNormal)
{
	// assuming that the voxel at voxelCenter is always opaque
	// marching in normal direction until we find an empty voxel
	vec3 ro = voxelCenter;
	vec3 rd = voxelNormal;

	ivec3 voxelCoord = worldSpacePositionToVoxelCoord(ro, 0);
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

	float t = 0.f;
	if (tMaxX < tMaxY && tMaxX < tMaxZ)
	{
		t = tMaxX;
	}
	else if (tMaxY < tMaxX && tMaxY < tMaxZ)
	{
		t = tMaxY;
	}
	else 
	{
		t = tMaxZ;
	}
	t += 0.01f;
	return voxelCenter + voxelNormal * t;
}

void main()
{
    seed = viewParameters.frameCount;
    flat_idx = int(gl_GlobalInvocationID.z * (u_imageSize.x * u_imageSize.y) + gl_GlobalInvocationID.y * u_imageSize.x + gl_GlobalInvocationID.x);

	ivec3 voxelCoord = ivec3(gl_GlobalInvocationID);
	float opacity = texelFetch(u_voxelGridOpacityTex, voxelCoord, 0).r;
	if (opacity <= 0.f)
		return;

	if (voxelCoord.x < u_imageSize.x && voxelCoord.y < u_imageSize.y && voxelCoord.z < u_imageSize.z)
	{
		vec3 voxelGridOrigin = u_voxelGridCenter + vec3(-u_voxelGridExtent.x, -u_voxelGridExtent.y, u_voxelGridExtent.z);
		vec3 voxelCenterPosition = voxelGridOrigin + vec3(voxelCoord + .5f) * vec3(u_voxelSize.xy, -u_voxelSize.z);
		vec3 voxelNormal = texelFetch(u_voxelGridNormalTex, voxelCoord, 0).rgb;

		vec3 rd = uniformSampleHemisphere(voxelNormal);

		// todo: is there better way to offset this
		vec2 rng = get_random();
		// randomly offset along normal
		// vec3 ro = voxelCenterPosition + voxelNormal * u_voxelSize.x * rng.x;
		vec3 ro = offsetRayStart(voxelCenterPosition, voxelNormal);
		// and then randomly offset along ray direction
		ro += rd * (0.1f * rng.y);

		vec3 radiance = vec3(0.f);
		HitRecord hitRecord = hierarchicalRayMarchingVoxelGrid(ro, rd);
		if (!hitRecord.bHit)
		{
			radiance = textureLod(u_skyCubemapTex, rd, 0).rgb;
		}

		vec3 irradiance = vec3(0.f);
		if (viewParameters.frameCount > 0)
		{
			if (viewParameters.frameCount < 1024)
			{
				irradiance = imageLoad(u_voxelGridIrradiance, voxelCoord).rgb * (viewParameters.frameCount - 1);
				irradiance += radiance;
				irradiance /= viewParameters.frameCount;
			}
			else
			{
				irradiance = imageLoad(u_voxelGridIrradiance, voxelCoord).rgb;
			}
		}
		else 
		{
			irradiance = radiance;
		}

		imageStore(u_voxelGridIrradiance, voxelCoord, vec4(irradiance, 0.f));

		ivec3 roVoxelCoord = worldSpacePositionToVoxelCoord(ro, 0);
		float roOpacity = texelFetch(u_voxelGridOpacityTex, roVoxelCoord, 0).r;
		if (roOpacity > 0.f)
		{
			// imageStore(u_voxelGridIrradiance, voxelCoord, vec4(1.f, 0.f, 0.f, 0.f));
		}
	}
}
