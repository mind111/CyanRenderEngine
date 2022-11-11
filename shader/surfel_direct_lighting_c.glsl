#version 450 core
#extension GL_NV_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable 
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
#define pi 3.14159265359

layout(std430, binding = 0) buffer ViewShaderStorageBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

struct Surfel {
	vec4 position;
	vec4 normal;
	vec4 albedo;
	vec4 radiance;
};

layout(std430, binding = 69) buffer SurfelBuffer {
	Surfel surfels[];
};

struct DirectionalShadowmap
{
	mat4 lightSpaceProjection;
	uint64_t depthTextureHandle;
} directionalShadowmap;

struct Cascade
{
	float n;
	float f;
	DirectionalShadowmap shadowmap;
} cascade;

struct CascadedShadowmap
{
	Cascade cascades[4];
} csm;

struct DirectionalLight
{
	vec4 direction;
	vec4 colorAndIntensity;
    mat4 lightSpaceView;
	CascadedShadowmap csm;
} directionalLight;

uniform struct PointLight
{
	vec3 position;
	vec4 colorAndIntensity;
	samplerCube shadowmap;
} pointLight;

uniform struct SkyLight
{
	samplerCube irradiance;
	samplerCube reflection;
} skyLightDef;

uniform struct SceneLights
{
    uint64_t BRDFLookupTexture;
	SkyLight skyLight;
	DirectionalLight directionalLight;
	// PointLight pointLights[MAX_NUM_POINT_LIGHTS];
} sceneLights;

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

/**
    * microfacet diffuse brdf
*/
vec3 LambertBRDF(vec3 baseColor) {
    return baseColor / pi;
}

float slopeBasedBias(vec3 n, vec3 l)
{
    float cosAlpha = max(dot(n, l), 0.f);
    float tanAlpha = tan(acos(cosAlpha));
    float bias = clamp(tanAlpha * 0.0001f, 0.f, 1.f);
    return bias;
}

float constantBias()
{
    return 0.0025f;
}

/**
	determine which cascade to sample from
*/
int calcCascadeIndex(in vec3 viewSpacePosition)
{
    int cascadeIndex = 0;
    for (int i = 0; i < 4; ++i)
    {
        if (viewSpacePosition.z < sceneLights.directionalLight.csm.cascades[i].f)
        {
            cascadeIndex = i;
            break;
        }
    }
    return cascadeIndex;
}

float calcBasicDirectionalShadow(vec3 worldSpacePosition, vec3 normal, in DirectionalLight directionalLight) {
    sampler2D sampler = sampler2D(directionalLight.csm.cascades[0].shadowmap.depthTextureHandle);
	float shadow = 0.0f;
    vec3 viewSpacePosition = (viewSsbo.view * vec4(worldSpacePosition, 1.f)).xyz;
    int cascadeIndex = calcCascadeIndex(viewSpacePosition);
    vec4 lightSpacePosition = directionalLight.csm.cascades[cascadeIndex].shadowmap.lightSpaceProjection * directionalLight.lightSpaceView * vec4(worldSpacePosition, 1.f);
    float depth = lightSpacePosition.z * .5f + .5f;
    vec2 uv = lightSpacePosition.xy * .5f + .5f;
#if SLOPE_BASED_BIAS
	float bias = constantBias() + slopeBasedBias(normal, directionalLight.direction.xyz);
#else
	float bias = constantBias();
#endif
	shadow = texture(sampler, uv).r < (depth - bias) ? 0.f : 1.f;
    return shadow;
}

float PCFShadow(vec3 worldSpacePosition, vec3 normal, in DirectionalLight directionalLight)
{
    sampler2D sampler = sampler2D(directionalLight.csm.cascades[0].shadowmap.depthTextureHandle);
	float shadow = 0.0f;
    vec2 texelOffset = vec2(1.f) / textureSize(sampler, 0);
    vec3 viewSpacePosition = (viewSsbo.view * vec4(worldSpacePosition, 1.f)).xyz;
    int cascadeIndex = calcCascadeIndex(viewSpacePosition);
    vec4 lightSpacePosition = directionalLight.csm.cascades[cascadeIndex].shadowmap.lightSpaceProjection * directionalLight.lightSpaceView * vec4(worldSpacePosition, 1.f);
    float depth = lightSpacePosition.z * .5f + .5f;
    vec2 uv = lightSpacePosition.xy * .5f + .5f;

    const int kernelRadius = 2;
    // 5 x 5 filter kernel
    float kernel[25] = {
        0.04, 0.04, 0.04, 0.04, 0.04,
        0.04, 0.04, 0.04, 0.04, 0.04,
        0.04, 0.04, 0.04, 0.04, 0.04,
        0.04, 0.04, 0.04, 0.04, 0.04,
        0.04, 0.04, 0.04, 0.04, 0.04
    };

    for (int i = -kernelRadius; i <= kernelRadius; ++i)
    {
        for (int j = -kernelRadius; j <= kernelRadius; ++j)
        {
            vec2 offset = vec2(i, j) * texelOffset;
            vec2 texCoord = uv + offset;
            if (texCoord.x < 0.f || texCoord.x > 1.f || texCoord.y < 0.f || texCoord.y > 1.f) 
            {
                continue;
			}
#if SLOPE_BASED_BIAS
			float bias = constantBias() + slopeBasedBias(normal, directionalLight.direction.xyz);
#else
			float bias = constantBias();
#endif
            float shadowSample = texture(sampler, texCoord).r < (depth - bias) ? 0.f : 1.f;
            shadow += shadowSample * kernel[(i + kernelRadius) * 5 + (j + kernelRadius)];
        }
    }
    return shadow;
}

float calcDirectionalShadow(vec3 worldSpacePosition, vec3 normal, in DirectionalLight directionalLight) {
    // disable PCF shadow for performance
    // return PCFShadow(worldSpacePosition, normal, directionalLight);
    return calcBasicDirectionalShadow(worldSpacePosition, normal, directionalLight);
}

void main() {
    vec3 position = surfels[gl_WorkGroupID.x].position.xyz;
    vec3 normal = surfels[gl_WorkGroupID.x].normal.xyz;
    vec3 albedo = surfels[gl_WorkGroupID.x].albedo.rgb;

    float shadow = calcDirectionalShadow(position, normal, sceneLights.directionalLight);
    float ndotl = max(dot(sceneLights.directionalLight.direction.xyz, normal), 0.f);
    vec3 li = sceneLights.directionalLight.colorAndIntensity.rgb * sceneLights.directionalLight.colorAndIntensity.a;
    vec3 radiance = LambertBRDF(albedo) * ndotl * li * shadow;
	surfels[gl_WorkGroupID.x].radiance = vec4(radiance, 1.f);
}
