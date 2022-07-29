#version 450 core

#extension GL_NV_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable 

struct PBRMaterial
{
	uint64_t diffuseMap;
	uint64_t normalMap;
	uint64_t metallicRoughnessMap;
	uint64_t occlusionMap;
	vec4 kAlbedo;
	vec4 kMetallicRoughness;
	uvec4 flags;
};

in VSOutput
{
	vec3 viewSpacePosition;
	vec3 worldSpacePosition;
	vec3 worldSpaceNormal;
	vec3 viewSpaceTangent;
	vec2 texCoord0;
	vec2 texCoord1;
    vec3 vertexColor;
    flat PBRMaterial material;
} psIn;

out vec3 outColor;

//- samplers
layout (binding = 0) uniform samplerCube skyboxDiffuse;
layout (binding = 1) uniform samplerCube skyboxSpecular;
layout (binding = 2) uniform sampler2D BRDFLookupTexture;
layout (binding = 5) uniform sampler2D ssaoTex;

#if 0
layout (binding = 10) uniform sampler3D sceneVoxelGridAlbedo;
layout (binding = 11) uniform sampler3D sceneVoxelGridNormal;
layout (binding = 12) uniform sampler3D sceneVoxelGridRadiance;
layout (binding = 13) uniform sampler3D sceneVoxelGridOpacity;
layout (binding = 14) uniform sampler2D vctxOcclusion;
layout (binding = 15) uniform sampler2D vctxIrradiance;
layout (binding = 16) uniform sampler2D vctxReflection;
#endif

// constants
#define pi 3.14159265359
#define SLOPE_BASED_BIAS 1
#define VIEW_SSBO_BINDING 0
#define VARIANCE_SHADOWMAP 0

layout(std430, binding = VIEW_SSBO_BINDING) buffer ViewShaderStorageBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

#if 0
//- voxel cone tracing
layout(std430, binding = 4) buffer VoxelGridData
{
    vec3 localOrigin;
    float voxelSize;
    int visMode;
    vec3 padding;
} sceneVoxelGrid;
#endif

//================================= "lights.glsl" =========================================
#define MAX_NUM_POINT_LIGHTS 32
#define NUM_SHADOW_CASCADES 4

#if VARIANCE_SHADOWMAP
uniform struct VarianceShadowmap
{
	mat4 lightSpaceProjection;
	sampler2D depthTexture;
	sampler2D depthSpuaredTexture;
};

uniform struct Cascade
{
	float n;
	float f;
	VarianceShadowmap shadowmap;
};
#else
// todo: since these following structs are declared to be uniform, they has to be instantiated on declaration, which is kind of 
// inconvenient, is there any better ways to store a texture as a struct member in glsl?
uniform struct DirectionalShadowmap
{
	mat4 lightSpaceProjection;
	sampler2D depthTexture;
} directionalShadowmap;

uniform struct Cascade
{
	float n;
	float f;
	DirectionalShadowmap shadowmap;
} cascade;
#endif

uniform struct CascadedShadowmap
{
	Cascade cascades[NUM_SHADOW_CASCADES];
} csm;

uniform struct DirectionalLight
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
} skyLight;

uniform struct SceneLights
{
	// SkyLight skyLight;
	DirectionalLight directionalLight;
	// PointLight pointLights[MAX_NUM_POINT_LIGHTS];
} sceneLights;

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

float PCFShadow(vec3 worldSpacePosition, vec3 normal, in DirectionalLight directionalLight)
{
	float shadow = 0.0f;
    vec2 texelOffset = vec2(1.f) / textureSize(directionalLight.csm.cascades[0].shadowmap.depthTexture, 0);
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
            float shadowSample = texture(directionalLight.csm.cascades[cascadeIndex].shadowmap.depthTexture, texCoord).r < (depth - bias) ? 0.f : 1.f;
            shadow += shadowSample * kernel[(i + kernelRadius) * 5 + (j + kernelRadius)];
        }
    }
    return shadow;
}

// todo: the shadow is almost too soft!
/** note:
    objects that are closer to the near clipping plane for each frusta in CSM doesn't show such 
    artifact, which infers that when t decreases, the Chebychev inequility increases. Don't know why this happens!
*/
#if 0
float varianceShadow(vec2 shadowTexCoord, float fragmentDepth, int cascadeOffset)
{
    float shadow = 1.f;
    float firstMoment = texture(shadowCascades[cascadeOffset], shadowTexCoord).r;
    if (firstMoment <= fragmentDepth)
    {
        float t = fragmentDepth;
        float secondMoment = texture(shadowCascades[cascadeOffset], shadowTexCoord).g;
        // use second and first moment to compute variance
        float variance = max(secondMoment - firstMoment * firstMoment, 0.0001f);
        //he Chebycv visibility test
        shadow = variance / (variance + (t - firstMoment) * (t - firstMoment));
    }
    return shadow;
}
#endif

float calcDirectionalShadow(vec3 worldPosition, vec3 normal, in DirectionalLight directionalLight)
{
    return PCFShadow(worldPosition, normal, directionalLight);
}
//==========================================================================

//=============== material.glsl =================================================
const uint kHasRoughnessMap         = 1 << 0;
const uint kHasMetallicMap          = 1 << 1;
const uint kHasMetallicRoughnessMap = 1 << 2;
const uint kHasOcclusionMap         = 1 << 3;
const uint kHasDiffuseMap           = 1 << 4;
const uint kHasNormalMap            = 1 << 5;
const uint kUsePrototypeTexture     = 1 << 6;
const uint kUseLightMap             = 1 << 7;            

/**
	mirror's pbr material definition on application side
*/
uniform struct MaterialInput
{
	uint M_flags;
	sampler2D M_albedo;
	sampler2D M_normal;
	sampler2D M_roughness;
	sampler2D M_metallic;
	sampler2D M_metallicRoughness;
	sampler2D M_occlusion;
	float M_kRoughness;
	float M_kMetallic;
	vec3 M_kAlbedo;
} materialInput;

/**
	material parameters processed and converted from application side material definition 
*/
struct MaterialParameters
{
	vec3 albedo;
	vec3 normal;
	float roughness;
	float metallic;
	float occlusion;
};

vec4 tangentSpaceToViewSpace(vec3 tn, vec3 vn, vec3 t) 
{
    mat4 tbn;
    // apply Gram-Schmidt process
    t = normalize(t - dot(t, vn) * vn);
    vec3 b = cross(vn, t);
    tbn[0] = vec4(t, 0.f);
    tbn[1] = vec4(b, 0.f);
    tbn[2] = vec4(vn, 0.f);
    tbn[3] = vec4(0.f, 0.f, 0.f, 1.f);
    return tbn * vec4(tn, 0.0f);
}

MaterialParameters getMaterialParameters(vec3 worldSpaceNormal, vec3 worldSpaceTangent, vec2 texCoord)
{
	MaterialParameters materialParameters;

    materialParameters.normal = worldSpaceNormal;
    if ((psIn.material.flags & kHasDiffuseMap) != 0u)
    {
        sampler2D sampler = sampler2D(psIn.material.normalMap);
        vec3 tn = texture(sampler, texCoord).xyz;
        // Convert from [0, 1] to [-1.0, 1.0] and renomalize if texture filtering changes the length
        tn = normalize(tn * 2.f - vec3(1.f));
        // Covert normal from tangent frame to camera space
        materialParameters.normal = tangentSpaceToViewSpace(tn, worldSpaceNormal, worldSpaceTangent).xyz;
    }

    materialParameters.albedo = psIn.material.kAlbedo.rgb;
    if ((psIn.material.flags & kHasDiffuseMap) != 0u)
    {
        sampler2D sampler = sampler2D(psIn.material.diffuseMap);
        materialParameters.albedo = texture(sampler, texCoord).rgb;
		// from sRGB to linear space if using a texture
		materialParameters.albedo = vec3(pow(materialParameters.albedo.r, 2.2f), pow(materialParameters.albedo.g, 2.2f), pow(materialParameters.albedo.b, 2.2f));
    }

    // According to gltf-2.0 spec, metal is sampled from b, roughness is sampled from g
    float roughness = psIn.material.kMetallicRoughness.g, metallic = psIn.material.kMetallicRoughness.r;
    if ((psIn.material.flags & kHasMetallicRoughnessMap) != 0u)
    {
        sampler2D sampler = sampler2D(psIn.material.metallicRoughnessMap);
        vec2 metallicRoughness = texture(sampler, texCoord).gb;
        roughness = metallicRoughness.x;
        roughness = roughness * roughness;
        metallic = metallicRoughness.y; 
    }
    else if ((psIn.material.flags & kHasRoughnessMap) != 0u)
    {
        roughness = texture(materialInput.M_roughness, texCoord).r;
        roughness = roughness * roughness;
        metallic = materialInput.M_kMetallic;
    } 
    materialParameters.roughness = roughness;
    materialParameters.metallic = metallic;

    materialParameters.occlusion = 1.f;
    if ((psIn.material.flags & kHasOcclusionMap) != 0)
    {
        sampler2D sampler = sampler2D(psIn.material.occlusionMap);
        materialParameters.occlusion = texture(sampler, texCoord).r;
		materialParameters.occlusion = pow(materialParameters.occlusion, 3.0f);
    }

	return materialParameters;
}
//===============================================================================

uniform sampler2D ssaoTexture;

mat3 tbn(vec3 n)
{
    vec3 up = abs(n.y) < 0.98f ? vec3(0.f, 1.f, 0.f) : vec3(0.f, 0.f, 1.f);
    vec3 right = cross(n, up);
    vec3 forward = cross(n, right);
    return mat3(
        right,
        forward,
        n
    );
}

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

// Taking the formula from the GGX paper and simplify to avoid computing tangent
// Using Disney's parameterization of alpha_g = roughness * roughness
float GGX(float roughness, float ndoth) 
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float result = alpha2;
    float denom = ndoth * ndoth * (alpha2 - 1.f) + 1.f;
    // prevent divide by infinity at the same time will produce 
    // incorrect specular highlight when roughness is really low.
    // when roughness is really low, specular highlights are supposed to be
    // really tiny and noisy.
    result /= max((pi * denom * denom), 0.0001f); 
    return result;
}

// TODO: implement fresnel that takes roughness into consideration
vec3 fresnel(vec3 f0, vec3 n, vec3 v)
{
    float ndotv = saturate(dot(n, v));
    float fresnelCoef = 1.f - ndotv;
    fresnelCoef = fresnelCoef * fresnelCoef * fresnelCoef * fresnelCoef * fresnelCoef;
    // f0: fresnel reflectance at incidence angle 0
    // f90: fresnel reflectance at incidence angle 90, f90 in this case is vec3(1.f) 
    return mix(f0, vec3(1.f), fresnelCoef);
}

/*
    lambda function in Smith geometry term
*/
float ggxSmithLambda(vec3 v, vec3 h, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float hdotv = saturate(dot(h, v));
    float hdotv2 = max(0.001f, hdotv * hdotv);
    // float a2 = hdotv2 / (alpha2 * (1.f - hdotv * hdotv));
    float a2Rcp = alpha2 * (1.f - hdotv2) / hdotv2;
    return 0.5f * (sqrt(1.f + a2Rcp) - 1.f);
}

/*
    * height-correlated Smith geometry attenuation 
*/
float ggxSmithG2(vec3 v, vec3 l, vec3 h, float roughness)
{
    float ggxV = ggxSmithLambda(v, h, roughness);
    float ggxL = ggxSmithLambda(l, h, roughness);
    return 1.f / (1.f + ggxV + ggxL);
}

/* 
    non-height correlated smith G2
*/
float ggxSmithG1(vec3 v, vec3 h, vec3 n, float roughness)
{
    float ndotv = dot(n, v); 
    float hdotv = dot(h, v); 
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float tangentTheta2 = (1 - ndotv * ndotv) / max(0.001f, (ndotv * ndotv));
    return 2.f / (1.f + sqrt(1.f + alpha2 * tangentTheta2));
}

float ggxSmithG2Ex(vec3 v, vec3 l, vec3 n, float roughness)
{
    vec3 h = normalize(v + n);
    return ggxSmithG1(v, h, n, roughness) * ggxSmithG1(l, h, n, roughness);
}

/**
    * microfacet diffuse brdf
*/
vec3 LambertBRDF(vec3 baseColor)
{
    return baseColor / pi;
}

/**
    * microfacet specular brdf 
    * A brdf fr(i, o, n) 
*/
vec3 CookTorranceBRDF(vec3 wi, vec3 wo, vec3 n, float roughness, vec3 f0)
{
    float ndotv = saturate(dot(n, wo));
    float ndotl = saturate(dot(n, wi));
    vec3 h = normalize(wi + wo);
    float ndoth = saturate(dot(n, h));
    float D = GGX(roughness, ndoth);
    float G = ggxSmithG2(wo, wi, n, roughness);
    // float G = ggxSmithG2Ex(wo, wi, n, roughness);
    vec3 F = fresnel(f0, n, wo);
    // TODO: the max() operator here actually affects how strong the specular highlight is
    // near grazing angle. Using a small value like 0.0001 will have pretty bad shading alias. However,
    // is using 0.1 here corret though. Need to reference other implementation. 
    return D * F * G / max((4.f * ndotv * ndotl), 0.01f);
}

/**
    f0 is specular reflectance
*/
vec3 calcF0(MaterialParameters materialParameters)
{
    return mix(vec3(0.04f), materialParameters.albedo, materialParameters.metallic);
}

vec3 calcDirectionalLight(in DirectionalLight directionalLight, MaterialParameters materialParameters, vec3 worldSpacePosition)
{
    vec3 radiance;
    float ndotl = max(dot(materialParameters.normal, directionalLight.direction.xyz), 0.f);
    // diffuse
    // todo: for some mysterious reason, this line is causing issue
    radiance += LambertBRDF(materialParameters.albedo) * directionalLight.colorAndIntensity.rgb * directionalLight.colorAndIntensity.a * ndotl;
    // specular
    vec3 viewSpacePosition = (viewSsbo.view * vec4(worldSpacePosition, 1.f)).xyz;
    vec3 viewDirection = normalize(-viewSpacePosition);
    vec3 f0 = calcF0(materialParameters);
    // radiance += CookTorranceBRDF(directionalLight.direction.xyz, viewDirection, materialParameters.normal, materialParameters.roughness, f0);
    // shadow
    // radiance *= calcDirectionalShadow(worldSpacePosition, materialParameters.normal, directionalLight);
    return radiance;
}

vec3 calcPointLight()
{
    vec3 radiance = vec3(0.f);
    return radiance;
}

#if 0
vec3 calcSkyLight(SkyLight skyLight, MaterialParameters materialParameters, vec3 worldSpacePosition)
{
    vec3 radiance = vec3(0.f);
    // irradiance
    radiance += LambertBRDF(materialParameters.albedo) * texture(skyLight.irradiance, materialParameters.normal).rgb;
    // reflection
    vec3 viewSpacePosition = (viewSsbo.view * vec4(worldSpacePosition, 1.f)).xyz;
    vec3 viewDirection = normalize(-viewSpacePosition);
    vec3 reflectionDirection = -reflect(viewDirection, materialParameters.normal);
    float ndotv = max(dot(materialParameters.normal, viewDirection), 0.f);
    mat4 view = viewSsbo.view;
    view[3] = vec4(0.f, 0.f, 0.f, 1.f);
    // transform the reflection vector back to world space
    vec3 worldSpaceReflectionDirection = (inverse(view) * vec4(reflectionDirection, 0.f)).xyz;
    vec3 BRDF = texture(BRDFLookupTexture, vec2(materialParameters.roughness, ndotv)).rgb; 
    vec3 f0 = calcF0(materialParameters);
    vec3 incidentRadiance = textureLod(skyboxSpecular, worldSpaceReflectionDirection, materialParameters.roughness * 10.f).rgb;
    radiance += incidentRadiance * (f0 * BRDF.r + BRDF.g);
    // todo: skylight occlusion
    float ssao = 1.f;
    if (viewSsbo.m_ssao > .5f)
    {
		vec2 screenTexCoord = gl_FragCoord.xy *.5f / vec2(textureSize(ssaoTex, 0));
		ssao = texture(ssaoTexture, screenTexCoord).r;
    }
    radiance *= ssao;
    return radiance;
}
#endif

vec3 calcLighting(SceneLights sceneLights, in MaterialParameters materialParameters, vec3 worldSpacePosition)
{
    vec3 radiance = vec3(0.f);

    // ambient light using flat shading
    float ndotl = dot(materialParameters.normal, normalize(-psIn.viewSpacePosition)) * .5 + .5f;
    radiance += vec3(0.15, 0.3, 0.5) * exp(0.01 * -length(psIn.viewSpacePosition)) * ndotl * materialParameters.albedo;
    // sun light
    radiance += calcDirectionalLight(sceneLights.directionalLight, materialParameters, worldSpacePosition);
    // radiance += calcSkyLight(sceneLights.skyLight, materialParameters, worldSpacePosition);
    return radiance;
}

void main()
{
    vec3 viewSpaceTangent = normalize(psIn.viewSpaceTangent);
    vec3 worldSpaceNormal = normalize(psIn.worldSpaceNormal);
    // todo: transform tangent back to world space
    MaterialParameters materialParameters = getMaterialParameters(worldSpaceNormal, viewSpaceTangent, psIn.texCoord0);
    outColor = calcLighting(sceneLights, materialParameters, psIn.worldSpacePosition);
    outColor *= length(psIn.vertexColor);
}
