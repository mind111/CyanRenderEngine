#version 450 core

#define PI 3.1415926

in VSOutput
{
	vec2 texCoord0;
} psIn;

layout (location = 0) out vec3 outIndirectIrradiance;

// todo: write a shader #include thing
// #import "random.csh"

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
uniform sampler2D sceneDepthBuffer;
uniform sampler2D prevFrameSceneDepthBuffer;
uniform sampler2D sceneNormalBuffer;
uniform sampler2D prevFrameIndirectIrradianceBuffer;
uniform sampler2D diffuseRadianceBuffer;

// todo: maybe using the same noises from the GTAO talk will produce better results?
uniform sampler2D blueNoise_16x16_R[8];
uniform sampler2D blueNoise_1024x1024_RGBA;

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

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) 
{
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
}

vec3 worldToScreen(vec3 pp, in mat4 view, in mat4 projection)
{
    vec4 p = projection * view * vec4(pp, 1.f);
    p /= p.w;
    return p.xyz * .5f + .5f;
}

struct HierarchicalZBuffer
{
	sampler2D depthQuadtree;
	int numMipLevels;
};
uniform HierarchicalZBuffer hiZ;

struct Settings 
{
	int kTracingStopMipLevel;
	int kMaxNumIterationsPerRay;
};
uniform Settings settings;

bool HiZTrace(in vec3 worldSpaceRO, in vec3 worldSpaceRD, inout float t)
{
    bool bHit = false;
	mat4 view = viewParameters.viewMatrix;
	mat4 projection = viewParameters.projectionMatrix;

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
    // int level = hiZ.numMipLevels - 1;
	int level = 2;
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

		// calculate height field intersection
		float minDepth = textureLod(hiZ.depthQuadtree, pp.xy, level).r;
		float tDepth;
        if (minDepth <= pp.z)
        {
            tDepth = (minDepth - pp.z);
        }
        else
        {
			tDepth = (minDepth - pp.z) / rd.z;
			tDepth = (tDepth < 0.f) ? (1.f / 0.f) : tDepth;
        }

		// calculate cell boundries intersection
		vec2 mipSize = vec2(textureSize(hiZ.depthQuadtree, level));
		vec2 texelSize = 1.f / mipSize;
		vec2 coord = pp.xy / texelSize;
        vec4 boundry = vec4(floor(coord.x), ceil(coord.x), floor(coord.y), ceil(coord.y));

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
			if (level <= 1)
            {
                if (tDepth > 0.f)
                {
					// todo: for some reason, tDepth can become infinity (divide by 0)
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
            float rayBump = 0.25f * length(rd.xy) * 1.f / min(textureSize(hiZ.depthQuadtree, 0).x, textureSize(hiZ.depthQuadtree, 0).y);
			// go up a level to perform more coarse trace
			screenSpaceT += (tCellBoundry + rayBump);
            level = min(level + 1, hiZ.numMipLevels - 1);
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
    seed = viewParameters.frameCount;
    flat_idx = int(floor(gl_FragCoord.y) * viewParameters.renderResolution.x + floor(gl_FragCoord.x));

	float deviceDepth = texture(sceneDepthBuffer, psIn.texCoord0).r; 
	vec3 n = texture(sceneNormalBuffer, psIn.texCoord0).rgb * 2.f - 1.f;

    if (deviceDepth > 0.999999f) discard;

	mat4 view = viewParameters.viewMatrix;
	mat4 prevFrameView = viewParameters.prevFrameViewMatrix;
	mat4 projection = viewParameters.projectionMatrix;
	mat4 prevFrameProjection = viewParameters.prevFrameProjectionMatrix;

	vec3 ro = screenToWorld(vec3(psIn.texCoord0, deviceDepth) * 2.f - 1.f, inverse(view), inverse(projection));
	
	vec3 indirectIrradiance = vec3(0.f);
	// for (int i = 0; i < 32; ++i)
	// {
		vec3 incidentRadiance = vec3(0.f);
		vec3 rd = uniformSampleHemisphere(n);
		float t;
		// make the default hit position really really far
		vec3 hitPosition = ro + 10000.f * rd;
		vec3 hitNormal = vec3(0.f);
		if (HiZTrace(ro, rd, t))
		{
			hitPosition = ro + t * rd;
			vec2 screenCoord = worldToScreen(hitPosition, view, projection).xy;
			hitNormal = texture(sceneNormalBuffer, screenCoord).rgb * 2.f - 1.f;
			// reject (false positives) backfacing samples that shouldn't contribute to indirect irradiance 
			bool bIsInUpperHemisphere = dot(hitNormal, -rd) > 0.f;
			if (bIsInUpperHemisphere)
			{
				float ndotl = max(dot(rd, n), 0.f);
				incidentRadiance = texture(diffuseRadianceBuffer, screenCoord).rgb * ndotl;
			}
		}
		indirectIrradiance += incidentRadiance;
	// }
	// indirectIrradiance /= float(32);

    if (viewParameters.frameCount > 0)
    {
		// temporal reprojection 
		// todo: take pixel velocity into consideration when doing reprojection
		// todo: consider changes in pixel neighborhood when reusing cache sample, changes in neighborhood pixels means SSAO value can change even there is a cache hit
		// todo: adaptive convergence-aware spatial filtering
		vec3 prevViewSpacePos = (prevFrameView * vec4(ro, 1.f)).xyz;
		vec4 prevNDCPos = prevFrameProjection * vec4(prevViewSpacePos, 1.f);
		prevNDCPos /= prevNDCPos.w;
		prevNDCPos.xyz = prevNDCPos.xyz * .5f + .5f;

		if (prevNDCPos.x <= 1.f && prevNDCPos.x >= 0.f && prevNDCPos.y <= 1.f && prevNDCPos.y >= 0.f)
		{
			float prevFrameDeviceZ = texture(prevFrameSceneDepthBuffer, prevNDCPos.xy).r;
			vec3 cachedPrevFrameViewSpacePos = (prevFrameView * vec4(screenToWorld(vec3(prevNDCPos.xy, prevFrameDeviceZ) * 2.f - 1.f, inverse(prevFrameView), inverse(prevFrameProjection)), 1.f)).xyz;
			float relativeDepthDelta = abs(cachedPrevFrameViewSpacePos.z - prevViewSpacePos.z) / -cachedPrevFrameViewSpacePos.z;
			vec3 indirectIrradianceHistory = texture(prevFrameIndirectIrradianceBuffer, prevNDCPos.xy).rgb;

			float smoothing = .9f;
			smoothing = clamp(smoothing * 0.05f / relativeDepthDelta, 0.f, smoothing);
			// based on how fast the camera/object moves to further refine the smoothing, maybe it's better to use screen space displacement instead
			vec3 cameraDisplacement = view[3].xyz - prevFrameView[3].xyz;
			smoothing = mix(smoothing, 0.f, clamp(length(cameraDisplacement) / 0.1f, 0.f, 1.f));
			indirectIrradiance = vec3(indirectIrradianceHistory * smoothing + incidentRadiance * (1.f - smoothing));
		}
	}

    outIndirectIrradiance = indirectIrradiance;
}
