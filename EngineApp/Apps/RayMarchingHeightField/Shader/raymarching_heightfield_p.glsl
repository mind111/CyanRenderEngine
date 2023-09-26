#version 450 core

#define PI 3.1415926

#define SNOW_LAYER 0
#define BASE_LAYER 1
#define INVALID_LAYER -1

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outColor;

// camera parameters
uniform vec3 cameraPosition;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform float n;
uniform float f;
uniform float fov;
uniform float aspectRatio;

uniform ivec2 renderResolution;
uniform int maxNumRayMarchingSteps;
uniform vec3 u_albedo;
uniform mat4 u_heightFieldTransformMatrix;

uniform float u_snowBlendMin;
uniform float u_snowBlendMax;
uniform vec3 u_groundAlbedo;
uniform vec3 u_snowAlbedo;
uniform sampler2D u_compositedHeightMap;
uniform sampler2D u_compositedNormalMap;
uniform sampler2D u_baseLayerHeightMap;
uniform sampler2D u_cloudOpacityMap;

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

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

const vec2 BlueNoiseInDisk[4] = vec2[4](
    vec2(0.478712,0.875764),
    vec2(-0.337956,-0.793959),
    vec2(-0.955259,-0.028164),
    vec2(0.864527,0.325689)
);

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

vec3 calcBaseLayerAlbedo(vec3 localSpacePosition, vec3 localSpaceNormal)
{
	vec3 outAlbedo;
	float snowHeightBlendFactor = smoothstep(u_snowBlendMin, u_snowBlendMax, localSpacePosition.y);
	float snowNormalBlendFactor = localSpaceNormal.y;
	float snowBlendFactor = snowHeightBlendFactor * snowNormalBlendFactor;
	outAlbedo = mix(u_groundAlbedo, u_snowAlbedo, snowBlendFactor);
	return outAlbedo;
}

vec3 calcHeightFieldAlbedo(vec3 localSpacePosition, vec3 localSpaceNormal, int layer)
{
	vec3 outAlbedo;
	if (layer == SNOW_LAYER)
	{
		outAlbedo = vec3(1.f);
	}
	else if (layer == BASE_LAYER)
	{
		outAlbedo = calcBaseLayerAlbedo(localSpacePosition, localSpaceNormal);
	}
	return outAlbedo;
}

vec3 backgroundColor()
{ 
	// todo: maybe use some fancy texture as background ...?
    vec2 rng2 = get_random().xy;
    float angle = rng2.x * PI * 2.f;
    mat2 perPixelRandomRotation = {
		{ cos(angle), sin(angle) },
		{ -sin(angle), cos(angle) }
    };

	vec2 coord = psIn.texCoord0 * 2.f - 1.f;
	float c = smoothstep(1.f, 0.f, length(coord) * .5f);

	vec2 centerTap = coord;

	// todo: dithering
	for (int i = 0; i < 4; ++i)
	{
		vec2 offset = perPixelRandomRotation * BlueNoiseInDisk[i] * .5f;
		vec2 sampleCoord = coord + offset;
		c += smoothstep(1.f, 0.f, length(sampleCoord) * .5f);
	}
	c /= 5.f;

	return vec3(0.1f) * c; 
}

struct HitRecord
{
	bool bHit;
	vec2 textureSpaceHit;
	vec3 worldSpaceHit;
	vec3 worldSpaceNormal;
	int numMarchedSteps;
	int layer;
	float cloudOpacity;
	int level;
	vec3 debugColor;
};

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

	return t;
}

// todo: do a seperate pass to reconstruct normal from scene depth
// todo: light pass with ao only
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
					outHitRecord.debugColor = vec3(0.f, 1.f, 0.f);
					outHitRecord.worldSpaceNormal = vec3(0.f, 1.f, 0.f);
				}
				// the ray hit the side
				else
				{
					textureSpaceHit = p;
					outHitRecord.debugColor = vec3(1.f, 0.f, 0.f);
					outHitRecord.worldSpaceNormal = vec3(1.f, 0.f, 0.f);
				}

				outHitRecord.textureSpaceHit = textureSpaceHit.xz;

				vec3 localSpaceHitPosition;
				localSpaceHitPosition.xz = vec2(textureSpaceHit.x, -textureSpaceHit.z) * 2.f + vec2(-1.f, 1.f);
				localSpaceHitPosition.y = textureSpaceHit.y;

				vec3 worldSpaceHitPosition = (u_heightFieldTransformMatrix * vec4(localSpaceHitPosition, 1.f)).xyz;
				outHitRecord.worldSpaceHit = worldSpaceHitPosition;
				outHitRecord.numMarchedSteps = i;
				outHitRecord.layer = SNOW_LAYER;  
				outHitRecord.cloudOpacity = 0.f;
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
			outHitRecord.level = currentLevel;
		}
	}

	return outHitRecord;
}

HitRecord linearRayMarchingHeightField(in vec3 localSpaceRo, in vec3 localSpaceRd)
{
	HitRecord outHitRecord;
	outHitRecord.bHit = false;
	outHitRecord.textureSpaceHit = vec2(0.f);
	outHitRecord.worldSpaceHit = vec3(0.f);
	outHitRecord.numMarchedSteps = 0;
	outHitRecord.layer = INVALID_LAYER;
	outHitRecord.cloudOpacity = 0.f;

	float t0, t1;
	if (intersectHeightFieldLocalSpaceAABB(localSpaceRo, localSpaceRd, t0, t1))
	{
		vec3 rayEntry = localSpaceRo + localSpaceRd * t0;
		float tMax = t1 - t0;

		// scuffed cloud rendering 
		const float cloudLayerHeight_0 = .5;
		// analytically solve the intersection with the cloud layer plane 0 in local space;
		float cloudLayerT_0 = (cloudLayerHeight_0 - rayEntry.y) / localSpaceRd.y;
		if (cloudLayerT_0 > 0.f &&  cloudLayerT_0 < tMax)
		{
			vec3 cloudLayerInteresction_0 = rayEntry + localSpaceRd * cloudLayerT_0;
			vec2 uv = cloudLayerInteresction_0.xz - vec2(-1.f, 1.f);
			uv.y *= -1.f;
			uv /= 2.f;
			float sceneHeight = texture(u_compositedHeightMap, uv).r;
			if (cloudLayerInteresction_0.y > sceneHeight)
			{
				float heightDelta = abs(sceneHeight - cloudLayerInteresction_0.y);
				// this helps reduce opacity around intersection between the cloud plane and the scene
				float opacityModiifer = smoothstep(0.f, 0.05f, heightDelta);
				outHitRecord.cloudOpacity = texture(u_cloudOpacityMap, uv).r * opacityModiifer;
			}
		}

		float stepSize = tMax / float(maxNumRayMarchingSteps);
		float t = 0.f;

		// linear ray marching into the height field
		for (int i = 0; i < maxNumRayMarchingSteps; ++i)
		{
			vec3 p = rayEntry + localSpaceRd * t;

			// flip z coordinates since z axis is going in -v direction in uv space
			vec2 uv = p.xz - vec2(-1.f, 1.f);
			uv.y *= -1.f;
			uv /= 2.f;

			float height = texture(u_compositedHeightMap, uv).r;
			float rayHeight = p.y;
			if (rayHeight <= height)
			{
				float baseLayerHeight = texture(u_baseLayerHeightMap, uv).r;
				// hit snow layer
				if (rayHeight > baseLayerHeight)
				{
					outHitRecord.layer = SNOW_LAYER;
				}
				// hit base layer
				else
				{
					outHitRecord.layer = BASE_LAYER;
				}

				vec3 localSpaceHitPosition = rayEntry + localSpaceRd * t;
				vec3 worldSpaceHitPosition = (u_heightFieldTransformMatrix * vec4(localSpaceHitPosition, 1.f)).xyz;

				outHitRecord.bHit = true;
				outHitRecord.textureSpaceHit = uv;
				outHitRecord.worldSpaceHit = worldSpaceHitPosition;
				outHitRecord.numMarchedSteps = i;
				if (cloudLayerT_0 < 0.f || cloudLayerT_0 >= t)
				{
					outHitRecord.cloudOpacity = 0.f;
				}
				break;
			}
			t += stepSize;
		}
	}
	outHitRecord.numMarchedSteps = maxNumRayMarchingSteps;

	return outHitRecord;
}

float calcDirectShadow(vec3 localSpaceRo, vec3 localSpaceRd)
{
	float shadow = 1.f;
	HitRecord hitRecord = linearRayMarchingHeightField(localSpaceRo, localSpaceRd);
	shadow = hitRecord.bHit ? 0.f : 1.f;
	return shadow;
}

const vec3 sunDir = normalize(vec3(-2.f, 0.5f, -2.f));
const vec3 sunColor = vec3(1.f, 0.5f, 0.25f) * 1.f;

#if 0
HitRecord castRay(in vec3 localSpaceRo, in vec3 localSpaceRd)
{
	HitRecord outHitRecord;
	outHitRecord.bHit = false;
	outHitRecord.textureSpaceHit = vec2(0.f);
	outHitRecord.worldSpaceHit = vec3(0.f);
	outHitRecord.numMarchedSteps = 0;
	outHitRecord.layer = INVALID_LAYER;

	float t0, t1;
	if (intersectHeightFieldLocalSpaceAABB(localSpaceRo, localSpaceRd, t0, t1))
	{
		vec3 rayEntry = localSpaceRo + localSpaceRd * t0;

		float tMax = t1 - t0;
		float stepSize = tMax / float(maxNumRayMarchingSteps);
		float t = 0.f;

		// linear ray marching into the height field
		for (int i = 0; i < maxNumRayMarchingSteps; ++i)
		{
		}
	}

	return outHitRecord;
}
#endif

vec3 calcSkyColor(vec3 d)
{
	vec3 horizonColor = vec3(1.f);
	vec3 skyColor = vec3(0.529, 0.708, 0.922);
	float c = smoothstep(-1.f, 0.5f, d.y);
	return mix(horizonColor, skyColor, c);
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

float calcAO(vec3 localSpacePosition, vec3 localSpaceNormal)
{
	float outAO = 0.f;
	const float numSamples = 16;
	vec3 ro = localSpacePosition + localSpaceNormal * 0.002f;
	for (int i = 0; i < numSamples; ++i)
	{
		vec3 rd = uniformSampleHemisphere(localSpaceNormal);
		HitRecord hitRecord = hierarchicalRayMarching(ro, rd);
		if (!hitRecord.bHit) 
		{
			outAO += max(dot(rd, localSpaceNormal), 0.f);
		}
	}
	outAO /= float(numSamples);
	return outAO;
}


vec3 calcDirectSkyLight(vec3 localSpacePosition, vec3 localSpaceNormal)
{
	const int numSamples = 8;
	vec3 color = vec3(0.f);
	vec3 ro = localSpacePosition + localSpaceNormal * 0.005f;
	for (int i = 0; i < numSamples; ++i)
	{
		vec3 rd = uniformSampleHemisphere(localSpaceNormal);
		// HitRecord hitRecord = linearRayMarchingHeightField(ro, rd);
		HitRecord hitRecord = hierarchicalRayMarching(ro, rd);
		if (!hitRecord.bHit) 
		{
			color += calcSkyColor(rd) * max(dot(rd, localSpaceNormal), 0.f);
		}
	}
	color /= float(numSamples);
	return color;
}

vec3 shadeHeightField(vec3 localSpaceRo, vec3 localSpaceRd)
{
	// vec3 color = backgroundColor();
	vec3 worldSpaceRd = normalize((u_heightFieldTransformMatrix * vec4(localSpaceRd, 0.f)).xyz);
	vec3 color = calcSkyColor(worldSpaceRd);
	// HitRecord hitRecord = linearRayMarchingHeightField(localSpaceRo, localSpaceRd);
	HitRecord hitRecord = hierarchicalRayMarching(localSpaceRo, localSpaceRd);
	if (hitRecord.bHit)
	{
		vec3 worldSpacePosition = hitRecord.worldSpaceHit;
		vec3 worldSpaceNormal = hitRecord.worldSpaceNormal;
		vec3 localSpacePosition = (inverse(u_heightFieldTransformMatrix) * vec4(worldSpacePosition, 1.f)).xyz;
		vec3 localSpaceNormal = hitRecord.worldSpaceNormal;

		vec3 albedo = calcHeightFieldAlbedo(localSpacePosition, localSpaceNormal, hitRecord.layer);
		// color = max(dot(n, sunDir), 0.f) * albedo * sunColor * 1.f;
		// color += calcDirectSkyLight(localSpacePosition, localSpaceNormal) * albedo;
		color = vec3(calcAO(localSpacePosition, localSpaceNormal));
		// color = vec3(localSpacePosition);
		// color = hitRecord.debugColor;

		vec4 clipSpacePos = projectionMatrix * viewMatrix * vec4(worldSpacePosition, 1.f);
		clipSpacePos /= clipSpacePos.w;
		gl_FragDepth = clipSpacePos.z * .5f + .5f;
	}
	else
	{
		gl_FragDepth = 1.f;
	}
	return color;
}

vec3 acesFilm(vec3 x) 
{
    //Aces film curve
    return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.,1.);
}

void main()
{
    seed = 0;
    flat_idx = int(floor(gl_FragCoord.y) * renderResolution.x + floor(gl_FragCoord.x));

	vec2 pixelCoord = psIn.texCoord0 * 2.f - 1.f;

	outColor = backgroundColor();

	vec3 ro = cameraPosition;
	// calculate view ray for current pixel
	vec3 o = cameraPosition + cameraForward * n;
	float d = n * tan(radians(fov / 2.f));
	vec3 u = cameraRight * d * aspectRatio;
	vec3 v = cameraUp * d;
	vec3 p = o + u * pixelCoord.x + v * pixelCoord.y;
	vec3 rd = normalize(p - cameraPosition);

	// transform the view ray from world space to height field's local space
	mat4 worldToHeightFieldLocal = inverse(u_heightFieldTransformMatrix);
	vec3 localSpaceRo = (worldToHeightFieldLocal * vec4(ro, 1.f)).xyz;
	vec3 localSpaceRd = (worldToHeightFieldLocal * vec4(rd, 0.f)).xyz;

	outColor = shadeHeightField(localSpaceRo, localSpaceRd);

	// tonemapping and gamma correction
	// outColor = acesFilm(outColor);
	// outColor = smoothstep(.0, 1.f, outColor);
	// outColor = pow(outColor, vec3(1.f / 2.2f));
}