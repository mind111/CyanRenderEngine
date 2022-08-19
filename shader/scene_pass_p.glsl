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
	vec3 worldSpaceTangent;
	flat float tangentSpaceHandedness;
	vec2 texCoord0;
	vec2 texCoord1;
    vec3 vertexColor;
    flat PBRMaterial material;
} psIn;

out vec3 outColor;

//- samplers
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
uniform uint64_t SSAO;
uniform uint64_t SSBN;
uniform float useBentNormal;

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
#endif

struct CascadedShadowmap
{
	Cascade cascades[NUM_SHADOW_CASCADES];
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

vec4 tangentSpaceToWorldSpace(vec3 tangent, vec3 bitangent, vec3 worldSpaceNormal, vec3 tangentSpaceNormal) 
{
    mat4 tbn;
    tbn[0] = vec4(tangent, 0.f);
    tbn[1] = vec4(bitangent, 0.f);
    tbn[2] = vec4(worldSpaceNormal, 0.f);
    tbn[3] = vec4(0.f, 0.f, 0.f, 1.f);
    return tbn * vec4(tangentSpaceNormal, 0.0f);
}

MaterialParameters getMaterialParameters(vec3 worldSpaceTangent, vec3 worldSpaceBitangent, vec3 worldSpaceNormal, vec2 texCoord)
{
	MaterialParameters materialParameters;

    materialParameters.normal = worldSpaceNormal;
    if ((psIn.material.flags & kHasNormalMap) != 0u)
    {
        sampler2D sampler = sampler2D(psIn.material.normalMap);
        vec3 tangentSpaceNormal = texture(sampler, vec2(texCoord.x, texCoord.y)).xyz;
        // convert from [0, 1] to [-1.0, 1.0] and renomalize if texture filtering changes the length
        tangentSpaceNormal = normalize(tangentSpaceNormal * 2.f - 1.f);
        // covert normal from tangent frame to world space
        materialParameters.normal = normalize(tangentSpaceToWorldSpace(worldSpaceTangent, worldSpaceBitangent, worldSpaceNormal, tangentSpaceNormal).xyz);
    }

    materialParameters.albedo = psIn.material.kAlbedo.rgb;
    if ((psIn.material.flags & kHasDiffuseMap) != 0u)
    {
        sampler2D sampler = sampler2D(psIn.material.diffuseMap);
        materialParameters.albedo = texture(sampler, texCoord).rgb;
		// convert color from sRGB to linear space if using a texture
		materialParameters.albedo = vec3(pow(materialParameters.albedo.r, 2.2f), pow(materialParameters.albedo.g, 2.2f), pow(materialParameters.albedo.b, 2.2f));
    }

    materialParameters.metallic = psIn.material.kMetallicRoughness.r;
    materialParameters.roughness = psIn.material.kMetallicRoughness.g; 
    if ((psIn.material.flags & kHasMetallicRoughnessMap) != 0u)
    {
        sampler2D sampler = sampler2D(psIn.material.metallicRoughnessMap);
		// according to gltf-2.0 spec, metal is sampled from b, roughness is sampled from g
        vec2 metallicRoughness = texture(sampler, texCoord).gb;
        materialParameters.roughness *= metallicRoughness.x;
        materialParameters.metallic *= metallicRoughness.y;
    }
	// materialParameters.roughness = pow(materialParameters.roughness, 2.f);

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
    result /= (pi * denom * denom) + 1e-6; 
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
	* non-height correlated Smith geometry term   
    * note: 
    this version follows LearnOpenGL.com
*/
float ggxSmithG1(vec3 v, vec3 h, vec3 n, float roughness)
{
    float a = pow(roughness, 2.f);
    float k = (a * a) / 2.0;
    float nom   = saturate(dot(n, v));
    float denom = saturate(dot(n, v)) * (1.0 - k) + k;
    return nom / max(denom, 0.001);
}

/*
	* non-height correlated Smith geometry term
    * note: 
    this version follows the formula listed in the GGX paper
*/
float ggxSmithG1Ex(vec3 v, vec3 h, vec3 n, float roughness)
{
    float hdotv = saturate(dot(h, v));
    float ndotv = saturate(dot(n, v));
    float tanThetaV2 = (1.f - ndotv * ndotv) / max((ndotv * ndotv), 0.1);
    // apply Disney' roughness remapping here to reduce "hot" specular, this should only be applied to analytical light sources 
    float a = pow((0.5 + roughness * .5), 2.f);
    return (hdotv / ndotv) > 0.f ? 2.f / (1.f + sqrt(1.f + a * a * tanThetaV2)) : 0.f;
}

// ----------------------------------------------------------------------------
float ggxSmithG(vec3 n, vec3 v, vec3 l, float roughness)
{
    vec3 h = normalize(v + l);
#if 0
    float ggx1 = ggxSmithG1(l, h, n, roughness);
    float ggx2 = ggxSmithG1(v, h, n, roughness);
#else
    float ggx1 = ggxSmithG1Ex(l, h, n, roughness);
    float ggx2 = ggxSmithG1Ex(v, h, n, roughness);
#endif
    return ggx1 * ggx2;
}

/**
    * microfacet diffuse brdf
*/
vec3 LambertBRDF(vec3 baseColor) {
    return baseColor / pi;
}

/**
    * brief: Disney's principled diffuse BRDF
    * increase retro-reflection at grazing angles for rough surface, while decrease diffuse reflectance when surface is smooth as most of the energy is reflected
    instead of refracted.
*/
vec3 DisneyDiffuseBRDF(float hdotl, float ndotl, float ndotv, MaterialParameters materialParameters)
{
#if 0
    float FD90 = 0.5f + 2.f * materialParameters.roughness * (hdotl * hdotl);
    // vec3 diffuseReflectance = LambertBRDF(materialParameters.albedo) * (1.f + (FD90 - 1.f) * pow((1 - ndotl), 5)) * (1.f + (FD90 - 1.f) * pow((1.f - ndotv), 5));
    vec3 diffuseReflectance = LambertBRDF(vec3(1.f)) * (1.f + (FD90 - 1.f) * pow((1 - ndotl), 5)) * (1.f + (FD90 - 1.f) * pow((1.f - ndotv), 5));
#else
    float roughness = materialParameters.roughness * materialParameters.roughness;
    float fl = pow((1.f - ndotl), 5.f);
    float fv = pow((1.f - ndotv), 5.f);
    float rr = (2.f * roughness) * (hdotl * hdotl);
    vec3 diffuseRetrorefl = LambertBRDF(materialParameters.albedo) * rr * (fl + fv + fl * fv * (rr - 1.f));
    vec3 diffuseReflectance = LambertBRDF(materialParameters.albedo) * (1.f - .5 * fl) * (1.f - .5 * fv) + diffuseRetrorefl;
#endif
    return diffuseReflectance;
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
    float G = ggxSmithG(n, wo, wi, roughness);
    vec3 F = fresnel(f0, n, wo);
    /**
    * the max() operator here actually affects how strong the specular highlight is
    * near grazing angle. Using a small value like 0.0001 will cause pretty bad shading aliasing.
    * using .1 here to suppress tiny specular highlights from flickering 
    */ 
    return D * F * G / ((4.f * ndotv * ndotl) + 1e-1);
}

/**
    f0 is specular reflectance
*/
vec3 calcF0(MaterialParameters materialParameters)
{
    return mix(vec3(0.04f), materialParameters.albedo, materialParameters.metallic);
}

vec3 DisneyBRDF(vec3 l, vec3 v, MaterialParameters materialParameters)
{
    vec3 BRDF;
    vec3 h = normalize(l + v);
    vec3 worldSpaceViewDirection = (inverse(viewSsbo.view) * vec4(normalize(-psIn.viewSpacePosition), 0.f)).xyz;
    float ndotv = saturate(dot(materialParameters.normal, worldSpaceViewDirection));
    float ndotl = max(dot(materialParameters.normal, directionalLight.direction.xyz), 0.f);
    float hdotl = saturate(dot(h, directionalLight.direction.xyz));

    vec3 f0 = calcF0(materialParameters);
    // dialectric
    vec3 dialectric = DisneyDiffuseBRDF(hdotl, ndotl, ndotv, materialParameters);
    dialectric += CookTorranceBRDF(l, v, materialParameters.normal, materialParameters.roughness, f0);
    // metallic
    vec3 metallic = CookTorranceBRDF(l, v, materialParameters.normal, materialParameters.roughness, f0);
    // lerp between the two
    return mix(dialectric, metallic, materialParameters.metallic);
}

vec3 calcDirectionalLight(in DirectionalLight directionalLight, MaterialParameters materialParameters, vec3 worldSpacePosition)
{
    vec3 radiance = vec3(0.f);
    // view direction in world space
    vec3 worldSpaceViewDirection = (inverse(viewSsbo.view) * vec4(normalize(-psIn.viewSpacePosition), 0.f)).xyz;
    float ndotl = max(dot(materialParameters.normal, directionalLight.direction.xyz), 0.f);
    vec3 f0 = calcF0(materialParameters);
    vec3 li = directionalLight.colorAndIntensity.rgb * directionalLight.colorAndIntensity.a;

    /** 
    * diffuse
    */
    vec3 h = normalize(directionalLight.direction.xyz + worldSpaceViewDirection);
    float hdotl = saturate(dot(h, directionalLight.direction.xyz));
    float ndotv = saturate(dot(materialParameters.normal, worldSpaceViewDirection));
    vec3 diffuse = mix((1.f - f0), vec3(0.f), materialParameters.metallic) * LambertBRDF(materialParameters.albedo);
    // vec3 diffuse = mix((1.f - f0), vec3(0.f), materialParameters.metallic) * DisneyDiffuseBRDF(hdotl, ndotl, ndotv, materialParameters);

    /** 
    * specular
    */
    vec3 specular = CookTorranceBRDF(directionalLight.direction.xyz, worldSpaceViewDirection, materialParameters.normal, materialParameters.roughness, f0);

    radiance += (diffuse + specular) * li * ndotl;

    // shadow
    radiance *= calcDirectionalShadow(worldSpacePosition, materialParameters.normal, directionalLight);
    return radiance;
}

vec3 calcPointLight()
{
    vec3 radiance = vec3(0.f);
    return radiance;
}

vec3 calcSkyLight(SkyLight skyLight, in MaterialParameters materialParameters, vec3 worldSpacePosition)
{
    vec3 radiance = vec3(0.f);

    vec3 worldSpaceViewDirection = (inverse(viewSsbo.view) * vec4(normalize(-psIn.viewSpacePosition), 0.0)).xyz;
    vec3 h = normalize(directionalLight.direction.xyz + worldSpaceViewDirection);
    float hdotl = saturate(dot(h, directionalLight.direction.xyz));
    float ndotv = saturate(dot(materialParameters.normal, worldSpaceViewDirection));

    vec3 f0 = calcF0(materialParameters);

    // irradiance
    vec2 pixelCoords = gl_FragCoord.xy / vec2(2560.f, 1440.f);
    vec3 bentNormal = texture(sampler2D(SSBN), pixelCoords).rgb * 2.f - 1.f;
    float ao = texture(sampler2D(SSAO), pixelCoords).r;

    vec3 diffuse = mix((1.f - f0), vec3(0.f), materialParameters.metallic) * DisneyDiffuseBRDF(hdotl, 1.f, ndotv, materialParameters);
    vec3 irradiance;
    if (useBentNormal > 0.5f)
		irradiance = texture(skyLight.irradiance, bentNormal).rgb;
	else
        irradiance = texture(skyLight.irradiance, materialParameters.normal).rgb;
	radiance += diffuse * irradiance * ao;

    // reflection
    vec3 reflectionDirection = -reflect(worldSpaceViewDirection, materialParameters.normal);
    vec3 BRDF = texture(sampler2D(sceneLights.BRDFLookupTexture), vec2(ndotv, materialParameters.roughness)).rgb; 
    vec3 incidentRadiance = textureLod(samplerCube(skyLight.reflection), reflectionDirection, materialParameters.roughness * 10.f).rgb;
    radiance += incidentRadiance * (f0 * BRDF.r + BRDF.g);
    return radiance;
}

vec3 calcLighting(SceneLights sceneLights, in MaterialParameters materialParameters, vec3 worldSpacePosition)
{
    vec3 radiance = vec3(0.f);

    // ambient light using flat shading
    // float ndotl = dot(materialParameters.normal, (inverse(viewSsbo.view) * vec4(normalize(-psIn.viewSpacePosition), 0.f)).xyz) * .5 + .5f;
    // radiance += vec3(0.15, 0.3, 0.5) * exp(0.01 * -length(psIn.viewSpacePosition)) * ndotl * materialParameters.albedo;

    // sun light
    radiance += calcDirectionalLight(sceneLights.directionalLight, materialParameters, worldSpacePosition);
    // sky light
    radiance += calcSkyLight(sceneLights.skyLight, materialParameters, worldSpacePosition);

    return radiance;
}

void main()
{
    vec3 worldSpaceTangent = normalize(psIn.worldSpaceTangent);
    vec3 worldSpaceNormal = normalize(psIn.worldSpaceNormal);
    worldSpaceTangent = normalize(worldSpaceTangent - dot(worldSpaceNormal, worldSpaceTangent) * worldSpaceNormal); 
    vec3 worldSpaceBitangent = normalize(cross(worldSpaceNormal, worldSpaceTangent)) * psIn.tangentSpaceHandedness;

    MaterialParameters materialParameters = getMaterialParameters(worldSpaceTangent, worldSpaceBitangent, worldSpaceNormal, psIn.texCoord0);
    outColor = calcLighting(sceneLights, materialParameters, psIn.worldSpacePosition);
}
