#version 450 core

#define PI 3.1415926

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outColor;

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
uniform sampler3D u_opacity;
uniform sampler3D u_radiance;
uniform vec3 u_voxelGridCenter;
uniform vec3 u_voxelGridExtent;
uniform vec3 u_voxelGridResolution;
uniform vec3 u_voxelGridAABBMin;
uniform vec3 u_voxelGridAABBMax;
uniform vec3 u_voxelSize;
uniform vec2 u_renderResolution;

uniform sampler2D u_sceneDepthTex; 
uniform sampler2D u_sceneNormalTex;
uniform float u_halfConeAperture;

uniform float u_coneNormalOffset;
uniform float u_coneDirectionOffset;
uniform float u_maxTraceDistance;

uniform samplerCube u_skyCubemap;
uniform float u_debugConeHalfAngle;
uniform sampler3D u_opacityHierarchy;

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

vec3 calcVoxelUV(vec3 p)
{
    vec3 o = u_voxelGridCenter + vec3(-u_voxelGridExtent.x, -u_voxelGridExtent.y, u_voxelGridExtent.z);
    vec3 uv = (p - o) / (u_voxelGridExtent * 2.f);
    uv.z *= -1.f;
	return uv;
}

bool isValidVoxelUV(vec3 uv)
{
	return (uv.x >= 0.f && uv.x <= 1.f && uv.y >= 0.f && uv.y <= 1.f && uv.z >= 0.f && uv.z <= 1.f);
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
				outHitRecord.mipLevel = 0;
				break;
			}

			if (currLevel >= u_numMipLevels || t >= (tExit - tEnter))
			{
				break;
			}

			// offset the ray in world space slightly when sampling the volume texture to avoid the
			// ray getting stuck at octree cell boundaries
			voxelCoord = worldSpacePositionToVoxelCoord(p + rd * 0.001, currLevel);

			float opacity = texelFetch(u_opacityHierarchy, voxelCoord, currLevel).r;
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

vec4 nearFieldConeTrace(vec3 ro, vec3 rd, float coneHalfAperture, float maxDistance, out float outMarchedDistance)
{
	vec4 acc = vec4(0.f);
	const float halfConeAperture = radians(coneHalfAperture);
	const float distanceToConeDiameter = 2.f * tan(halfConeAperture);
	float stepSize = .5f / 4.f;
	float d = stepSize;
	vec3 p = ro + rd * d;
	float lod = 0.f;
	while (d < maxDistance)
	{
		if (acc.a >= 1.f || lod > u_numMipLevels)
		{
			break;
		}

		float coneDiameter = d * distanceToConeDiameter;
		stepSize = coneDiameter;
		vec3 voxelUV = calcVoxelUV(p);
		if (isValidVoxelUV(voxelUV))
		{
			lod = max(log2(coneDiameter / u_voxelSize.x), 0.f);
			float opacity = textureLod(u_opacity, voxelUV, lod).r;
			if (opacity > 0.f)
			{
				vec3 radiance = textureLod(u_radiance, voxelUV, lod).rgb;
				acc.rgb += (1.f - acc.a) * radiance;
				acc.a += (1.f - acc.a) * opacity;
			}
		}

		p += rd * stepSize;
		d += stepSize;
	}

	outMarchedDistance = min(d, maxDistance);
	return acc;
}

vec4 coneTrace(vec3 ro, vec3 rd, float coneHalfAperture, float maxDistance)
{
	vec3 p = ro + rd * u_voxelSize.x;
	vec4 acc = vec4(0.f);
	const float halfConeAperture = radians(coneHalfAperture);
	const float distanceToConeDiameter = 2.f * tan(halfConeAperture);

	float tEnter, tExit;
	if (intersectAABB(ro, rd, tEnter, tExit, u_voxelGridAABBMin, u_voxelGridAABBMax))
	{
		float d = max(u_voxelSize.x, tEnter);
		float lod = 0.f;
		while (d < maxDistance)
		{
			if (acc.a >= 1.f || d > (tExit - tEnter) || lod > 7.f)
			{
				break;
			}

			float coneDiameter = d * distanceToConeDiameter;
#if 1 // cone diameter based stepping
			float stepSize = coneDiameter;
#else // linear stepping
			float stepSize = (tExit - tEnter) / 128.f;
#endif
			vec3 voxelUV = calcVoxelUV(p);
			if (isValidVoxelUV(voxelUV))
			{
				lod = max(log2(coneDiameter / u_voxelSize.x), 0.f);
				float opacity = textureLod(u_opacity, voxelUV, lod).r;
				if (opacity > 0.f)
				{
					vec3 radiance = textureLod(u_radiance, voxelUV, lod).rgb;
					acc.rgb += (1.f - acc.a) * radiance;
					acc.a += (1.f - acc.a) * opacity;
				}
			}

			p += rd * stepSize;
			d += stepSize;
		}
	}

	// sample sky light
	if (acc.a < 1.f)
	{
		// acc.rgb += (1.f - acc.a) * texture(u_skyCubemap, rd).rgb;
	}

	return acc;
}

// diffuse cone setup #1
vec2 coneAngles[7] = { 
	vec2(0.f, 0.f),
	vec2(30.f, 60.f),
	vec2(30.f, 120.f),
	vec2(30.f, 180.f),
	vec2(30.f, 240.f),
	vec2(30.f, 300.f),
	vec2(30.f, 360.f)
};

// diffuse cone setup #2
vec3 diffuseConeDirections[6] = {	
	vec3(0.f, 1.f, 0.f),
	vec3(0.f, .5f, 0.866025f),
	vec3(0.823639, 0.5, 0.267617),
	vec3(0.509037, 0.5, -0.700629),
	vec3(-0.509037, 0.5f, -0.700629),
	vec3(-0.823639, .5f, 0.267617)
};

// todo: sample sky light with proper miplevel
vec3 coneTraceDiffuse(vec3 p, vec3 n)
{
	const float diffuseConeHalfAperture = 30.f;
	vec3 ro = p + n * u_coneNormalOffset;
	vec3 irradiance = vec3(0.f);
	for (int i = 0; i < 6; ++i)
	{
		vec2 coneAngle = coneAngles[i];
		vec3 dir = construct_ONB_frisvad(n) * normalize(vec3(diffuseConeDirections[i].x, diffuseConeDirections[i].z, diffuseConeDirections[i].y));
		float ndotl = max(dot(n, dir), 0.f);
		irradiance += coneTrace(ro, dir, diffuseConeHalfAperture, u_maxTraceDistance).rgb * ndotl;
	}
	irradiance /= 6.f;
	return irradiance;
}

uniform float u_nearFieldConeTraceDistance;
uniform float u_nearFieldConeTraceAperture;

// todo: cone marching switching to faithful voxel ray marching
vec3 hybridTraceDiffuse(vec3 p, vec3 n)
{
	vec3 outIrradiance = vec3(0.f);
	int numSamples = 32;
	#if 1
	// cone trace .5 meter first
	for (int i = 0; i < numSamples; ++i)
	{
		vec3 co = p + n * u_coneNormalOffset;
		vec3 rd = uniformSampleHemisphere(n);
		float d = 0.f;
		vec4 coneAcc = nearFieldConeTrace(co, rd, u_nearFieldConeTraceAperture, u_nearFieldConeTraceDistance, d);

		// then connect with faithful voxel ray marching
		if (coneAcc.a < 1.f)
		{
			vec3 ro = co + rd * d;
			HitRecord hitRecord = hierarchicalRayMarchingVoxelGrid(ro, rd);
			vec3 radiance = vec3(0.f);
			if (hitRecord.bHit)
			{
				radiance = textureLod(u_radiance, calcVoxelUV(hitRecord.worldSpacePosition), 0.f).rgb;
			}
			else
			{
				radiance = texture(u_skyCubemap, rd).rgb;
			}
			coneAcc.rgb += (1.f - coneAcc.a) * radiance * max(dot(n, rd), 0.f);
		}

		outIrradiance += coneAcc.rgb;
	}
	#else
	for (int i = 0; i < numSamples; ++i)
	{
		vec3 ro = p + n * u_coneNormalOffset;
		vec3 rd = uniformSampleHemisphere(n);
		HitRecord hitRecord = hierarchicalRayMarchingVoxelGrid(ro, rd);
		if (hitRecord.bHit)
		{
			vec3 radiance = textureLod(u_radiance, calcVoxelUV(hitRecord.worldSpacePosition), 0.f).rgb;
			outIrradiance += radiance;
		}
	}
	#endif
	outIrradiance /= numSamples;

	return outIrradiance;
}

vec3 debugTraceReflection(vec3 p, vec3 n)
{
	vec3 viewDirection = normalize(viewParameters.cameraPosition - p);
	vec3 r = -reflect(viewDirection, n);
	vec3 radiance = coneTrace(p + n * u_coneNormalOffset, r, max(.1f, u_debugConeHalfAngle), u_maxTraceDistance).rgb;
	return radiance;
}

void main()
{	
    seed = 0;
    flat_idx = int(floor(gl_FragCoord.y) * viewParameters.renderResolution.x + floor(gl_FragCoord.x));

	float depth = texture(u_sceneDepthTex, psIn.texCoord0).r;
	if (depth > .999999f) 
	{
		discard;
	}

	vec3 n = normalize(texture(u_sceneNormalTex, psIn.texCoord0).rgb * 2.f - 1.f);
	mat4 view = viewParameters.viewMatrix, projection = viewParameters.projectionMatrix;
	vec3 worldSpacePosition = screenToWorld(vec3(psIn.texCoord0, depth) * 2.f - 1.f, inverse(view), inverse(projection));
	// vec3 indirectRadiance = coneTraceDiffuse(worldSpacePosition, n).rgb;
	// vec3 indirectRadiance = debugTraceReflection(worldSpacePosition, n);
	vec3 indirectRadiance = hybridTraceDiffuse(worldSpacePosition, n).rgb;
	outColor = indirectRadiance;
}
