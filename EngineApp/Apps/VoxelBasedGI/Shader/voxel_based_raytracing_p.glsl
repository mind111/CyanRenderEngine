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
uniform sampler2D u_sceneDepthTex;
uniform sampler2D u_sceneNormalTex;
uniform sampler2D u_sceneMetallicRoughnessTex;
uniform sampler2D u_sceneAlbedoTex;
uniform vec3 u_voxelGridCenter;
uniform vec3 u_voxelGridExtent;
uniform vec3 u_voxelGridResolution;
uniform vec3 u_voxelGridAABBMin;
uniform vec3 u_voxelGridAABBMax;
uniform vec3 u_voxelSize;
uniform sampler3D u_voxelGridOpacityTex;
uniform sampler3D u_voxelGridNormalTex;
uniform sampler3D u_voxelGridRadianceTex;
uniform samplerCube u_skyCubemapTex;
uniform sampler2D u_prevFrameSceneDepthTex;
uniform sampler2D u_prevFrameVBRTTex;
uniform vec3 u_sunDirection;
uniform vec3 u_sunColor;
uniform vec2 u_renderResolution;
uniform float u_cameraNearClippingPlane; 
uniform float u_cameraFov; 

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
	return construct_ONB_frisvad(n) * normalize(vec3(uv.xy, z));
}

vec3 importanceSampleGGX(vec3 n, float roughness, vec2 xi)
{
	float a = roughness;

	float phi = 2 * PI * xi.x;
	float cosTheta = sqrt((1 - xi.y) / ( 1 + (a*a - 1) * xi.y));
	float sinTheta = sqrt(1 - cosTheta * cosTheta);

	vec3 h;
	h.x = sinTheta * cos( phi );
	h.y = sinTheta * sin( phi );
	h.z = cosTheta;

	vec3 up = abs(n.z) < 0.999 ? vec3(0, 0.f, 1.f) : vec3(1.f, 0, 0.f);
	vec3 tangentX = normalize( cross( up, n ) );
	vec3 tangentY = cross( n, tangentX);
	// tangent to world space
	return tangentX * h.x + tangentY * h.y + n * h.z;
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

float GGX(float roughness, float ndoth) 
{
    float alpha = roughness;
    float alpha2 = alpha * alpha;
    float result = max(alpha2, 1e-6); // prevent the nominator goes to 0 when roughness equals 0
    float denom = ndoth * ndoth * (alpha2 - 1.f) + 1.f;
    result /= (PI * denom * denom); 
    return result;
}

vec3 fresnel(vec3 f0, float vdoth)
{
    float fresnelCoef = 1.f - vdoth;
    fresnelCoef = fresnelCoef * fresnelCoef * fresnelCoef * fresnelCoef * fresnelCoef;
    // f0: fresnel reflectance at incidence angle 0
    // f90: fresnel reflectance at incidence angle 90, f90 in this case is vec3(1.f) 
    vec3 f90 = vec3(1.f);
    return mix(f0, f90, fresnelCoef);
}

float V_SmithGGXHeightCorrelated(float ndotv, float ndotl, float roughness)
{
    float a2 = roughness * roughness;
    float v = ndotl * sqrt(ndotv * ndotv * (1.0 - a2) + a2);
    float l = ndotv * sqrt(ndotl * ndotl * (1.0 - a2) + a2);
    return 0.5f / max((v + l), 1e-5);
}

vec3 offsetRayStart(vec3 p, vec3 n)
{
	// assuming that the voxel at voxelCenter is always opaque
	// marching in normal direction until we find an empty voxel
	vec3 ro = p;
	vec3 rd = n;

	vec3 voxelGridOrigin = u_voxelGridCenter + vec3(-u_voxelGridExtent.x, -u_voxelGridExtent.y, u_voxelGridExtent.z);

	vec3 d = (p - voxelGridOrigin);
	d.z *= -1.f;
	vec3 uv = d / (u_voxelGridExtent * 2.f);
	ivec3 voxelCoord = ivec3(floor(uv * u_voxelGridResolution));

	int stepX = int(sign(rd.x));
	int stepY = int(sign(rd.y));
	int stepZ = int(sign(rd.z));

	float tDeltaX = u_voxelSize.x / abs(rd.x);
	float tDeltaY = u_voxelSize.y / abs(rd.y);
	float tDeltaZ = u_voxelSize.z / abs(rd.z);

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
	for (int i = 0; i < 8; ++i)
	{
		// if opaque, keep marching
		float opacity = texelFetch(u_voxelGridOpacityTex, voxelCoord, 0).r;
		if (opacity < 0.5f)
		{
			break;
		}

		if (tMaxX < tMaxY && tMaxX < tMaxZ)
		{
			t = tMaxX;
			tMaxX += tDeltaX;
			voxelCoord += ivec3(stepX, 0, 0);
		}
		else if (tMaxY < tMaxX && tMaxY < tMaxZ)
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
	t += 0.01f;
	return p + n * t;
}

void main()
{
    seed = viewParameters.frameCount;
    flat_idx = int(floor(gl_FragCoord.y) * viewParameters.renderResolution.x + floor(gl_FragCoord.x));

	float depth = texture(u_sceneDepthTex, psIn.texCoord0).r;
	if (depth > .999999f) 
	{
		discard;
	}

	vec3 n = normalize(texture(u_sceneNormalTex, psIn.texCoord0).rgb * 2.f - 1.f);
	mat4 view = viewParameters.viewMatrix, projection = viewParameters.projectionMatrix;
	vec3 worldSpacePosition = screenToWorld(vec3(psIn.texCoord0, depth) * 2.f - 1.f, inverse(view), inverse(projection));
	vec3 viewDirection = -normalize(worldSpacePosition - viewParameters.cameraPosition);
	vec3 albedo = texture(u_sceneAlbedoTex, psIn.texCoord0).rgb;
	vec3 MRO = texture(u_sceneMetallicRoughnessTex, psIn.texCoord0).rgb;
	float metallic = MRO.r;
	float roughness = MRO.g;
	vec3 diffuseAlbedo = (1.f - MRO.r) * albedo;
	vec3 specularAlbedo = mix(vec3(0.04), albedo, MRO.r);

	vec3 outRadiance = vec3(0.f);

	// direct lighting
	{
	#if 0
		vec3 directRadiance = vec3(0.f);
		vec3 shadowRo = worldSpacePosition + n * 0.03f;
		vec3 shadowRd = u_sunDirection;
		HitRecord directShadowHitRecord = hierarchicalRayMarchingVoxelGrid(shadowRo, shadowRd);
		float shadow = directShadowHitRecord.bHit ? 0.f : 1.f;

		// diffuse
		outRadiance += u_sunColor * (diffuseAlbedo / PI) * max(dot(n, u_sunDirection), 0.f) * shadow;

		// specular
		vec3 h = normalize(viewDirection + n);
		float ndoth = clamp(dot(n, h), 0.f, 1.f);
		float ndotl = max(dot(n, u_sunDirection), 0.f);
		float ndotv = max(dot(n, viewDirection), 0.f);
		float vdoth = clamp(dot(viewDirection, h), 0.f, 1.f);
		float D = GGX(roughness * roughness, ndoth);
		float V = V_SmithGGXHeightCorrelated(ndotv, ndotl, roughness * roughness);
		vec3 F = fresnel(specularAlbedo, vdoth);
		outRadiance += u_sunColor * max(dot(n, u_sunDirection), 0.f) * shadow * D * F * V;
	#endif
	}

	// indirect lighting
	// combining offset along normal as well as ray direction helps a lot with correcting self intersection
	const float normalOffset = .1f, rayOffset = u_voxelSize.x;
	vec3 ro = worldSpacePosition + n * normalOffset;

	// diffuse
	{
		vec2 rng = get_random();
		vec3 diffuseRadiance = vec3(0.f);
		vec3 rd = uniformSampleHemisphere(n);
		HitRecord hitRecord = hierarchicalRayMarchingVoxelGrid(ro + rd * rayOffset, rd);
		float ndotl = max(dot(n, rd), 0.f);
		if (!hitRecord.bHit)
		{
			diffuseRadiance = textureLod(u_skyCubemapTex, rd, 0).rgb * ndotl;
		}
		else
		{
			vec3 hitRadiance = texelFetch(u_voxelGridRadianceTex, hitRecord.voxelCoord, 0).rgb;
			diffuseRadiance = hitRadiance * ndotl;
		}
		outRadiance += diffuseRadiance * diffuseAlbedo;
	}

	// reflection cannot be naively accmulated since it's view dependent
	// specular
	{
	#if 0
		vec3 reflectionRadiance = vec3(0.f);
		vec3 h = importanceSampleGGX(n, MRO.g * MRO.g, get_random());
		vec3 rd = -reflect(viewDirection, h);
		HitRecord hitRecord = hierarchicalRayMarchingVoxelGrid(ro + rd * rayOffset, rd);
		float ndotl = max(dot(n, rd), 0.f);
		if (!hitRecord.bHit)
		{
			reflectionRadiance = textureLod(u_skyCubemapTex, rd, 0).rgb;
		}
		else
		{
			vec3 worldSpaceHitPos = hitRecord.worldSpacePosition;
			vec3 hitRadiance = texelFetch(u_voxelGridRadianceTex, hitRecord.voxelCoord, 0).rgb;
			reflectionRadiance = hitRadiance * ndotl;
		}

		float ndotv = clamp(dot(n, viewDirection), 0.f, 1.f);
		float vdoth = clamp(dot(viewDirection, h), 0.f, 1.f);
		float ndoth = clamp(dot(n, h), 0.f, 1.f);
		float V = V_SmithGGXHeightCorrelated(ndotv, ndotl, MRO.g * MRO.g);
		vec3 F = fresnel(specularAlbedo, vdoth);
		outRadiance += reflectionRadiance * V * F * vdoth / (ndoth * ndotv);
	#endif
	}

	outColor = outRadiance;

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
				vec3 shadingHistory = texture(u_prevFrameVBRTTex, prevNDCPos.xy).rgb;
				outColor = vec3(shadingHistory * .9f + outRadiance * .1f);
			}
		}
	}
}
