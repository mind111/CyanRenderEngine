#version 450 core

#extension GL_NV_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable 

// header block, each shader file should only have one header block
// = begin headers =
// #include: lights.glsl
// #include: lighting.glsl
// #include: materials.glsl
// = end headers =

const uint kHasAlbedoMap            = 1 << 0;
const uint kHasNormalMap            = 1 << 1;
const uint kHasMetallicRoughnessMap = 1 << 2;
const uint kHasOcclusionMap         = 1 << 3;

/**
	mirror's the material definition on application side
    struct GpuMaterial {
        u64 albedoMap;
        u64 normalMap;
        u64 metallicRoughnessMap;
        u64 occlusionMap;
        glm::vec4 albedo = glm::vec4(.9f, .9f, .9f, 1.f);
        f32 metallic = 0.f;
        f32 roughness = .5f;
        f32 emissive = 1.f;
        u32 flag = 0u;
    };
*/
struct MaterialDesc {
	uint64_t albedoMap;
	uint64_t normalMap;
	uint64_t metallicRoughnessMap;
    uint64_t occlusionMap;
    vec4 albedo;
    float metallic;
    float roughness;
    float emissive;
    uint flag;
};

in VSOutput {
	vec3 viewSpacePosition;
	vec3 worldSpacePosition;
	vec3 worldSpaceNormal;
	vec3 worldSpaceTangent;
	flat float tangentSpaceHandedness;
	vec2 texCoord0;
	vec2 texCoord1;
    vec3 vertexColor;
    flat MaterialDesc desc;
} psIn;

out vec3 outColor;

// constants
#define pi 3.14159265359
#define SLOPE_BASED_BIAS 0

layout(std430) buffer ViewBuffer 
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

//================================= "lights.glsl" =========================================
const uint kNumShadowCascades = 4;
struct DirectionalShadowMap {
	mat4 lightSpaceView;
	mat4 lightSpaceProjection;
	uint64_t depthTextureHandle;
	vec2 padding;
};

struct Cascade {
	float n;
	float f;
	vec2 padding;
	DirectionalShadowMap shadowMap;
};

struct CascadedShadowMap {
	Cascade cascades[kNumShadowCascades];
};

struct DirectionalLight {
	vec4 colorAndIntensity;
	vec4 direction;
	CascadedShadowMap csm;
};

layout (std430) buffer DirectionalLightBuffer {
	DirectionalLight directionalLights[];
};

uniform struct SkyLight {
	float intensity;
    uint64_t BRDFLookupTexture;
	samplerCube irradiance;
	samplerCube reflection;
} skyLight;
//========================================================================================

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
int calcCascadeIndex(in vec3 viewSpacePosition, in DirectionalLight directionalLight)
{
    int cascadeIndex = 0;
    for (int i = 0; i < 4; ++i)
    {
        if (viewSpacePosition.z < directionalLight.csm.cascades[i].f)
        {
            cascadeIndex = i;
            break;
        }
    }
    return cascadeIndex;
}

float PCFShadow(vec3 worldSpacePosition, vec3 normal, in DirectionalLight directionalLight)
{
    sampler2D sampler = sampler2D(directionalLight.csm.cascades[0].shadowMap.depthTextureHandle);
	float shadow = 0.0f;
    vec2 texelOffset = vec2(1.f) / textureSize(sampler, 0);
    vec3 viewSpacePosition = (viewSsbo.view * vec4(worldSpacePosition, 1.f)).xyz;
    int cascadeIndex = calcCascadeIndex(viewSpacePosition, directionalLight);
    vec4 lightSpacePosition = 
		directionalLight.csm.cascades[cascadeIndex].shadowMap.lightSpaceProjection 
	  * directionalLight.csm.cascades[cascadeIndex].shadowMap.lightSpaceView * vec4(worldSpacePosition, 1.f);
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

float calcDirectionalShadow(vec3 worldPosition, vec3 normal, in DirectionalLight directionalLight)
{
    return PCFShadow(worldPosition, normal, directionalLight);
}
//==========================================================================

//=============== material.glsl =================================================
/**
	material converted from application side material definition 
*/
struct Material 
{
	vec3 albedo;
	vec3 normal;
	float roughness;
	float metallic;
	float occlusion;
};

vec4 tangentSpaceToWorldSpace(vec3 tangent, vec3 bitangent, vec3 worldSpaceNormal, vec3 tangentSpaceNormal) {   
    mat4 tbn;
    tbn[0] = vec4(tangent, 0.f);
    tbn[1] = vec4(bitangent, 0.f);
    tbn[2] = vec4(worldSpaceNormal, 0.f);
    tbn[3] = vec4(0.f, 0.f, 0.f, 1.f);
    return tbn * vec4(tangentSpaceNormal, 0.f);
}

Material getMaterial(in MaterialDesc desc, vec3 worldSpaceNormal, vec3 worldSpaceTangent, vec3 worldSpaceBitangent, vec2 texCoord) {
    Material outMaterial;
    
    outMaterial.normal = worldSpaceNormal;
    if ((desc.flag & kHasNormalMap) != 0u) {
        vec3 tangentSpaceNormal = texture(sampler2D(desc.normalMap), texCoord).xyz;
        // Convert from [0, 1] to [-1.0, 1.0] and renomalize if texture filtering changes the length
        tangentSpaceNormal = normalize(tangentSpaceNormal * 2.f - 1.f);
        // Covert normal from tangent frame to camera space
        outMaterial.normal = normalize(tangentSpaceToWorldSpace(worldSpaceTangent, worldSpaceBitangent, worldSpaceNormal, tangentSpaceNormal).xyz);
    }

    outMaterial.albedo = desc.albedo.rgb;
    if ((desc.flag & kHasAlbedoMap) != 0u) {
        outMaterial.albedo = texture(sampler2D(desc.albedoMap), texCoord).rgb;
		// from sRGB to linear space if using a texture
		outMaterial.albedo = vec3(pow(outMaterial.albedo.r, 2.2f), pow(outMaterial.albedo.g, 2.2f), pow(outMaterial.albedo.b, 2.2f));
    }

    // According to gltf-2.0 spec, metal is sampled from b, roughness is sampled from g
    float roughness = desc.roughness, metallic = desc.metallic;
    if ((desc.flag & kHasMetallicRoughnessMap) != 0u)
    {
        vec2 metallicRoughness = texture(sampler2D(desc.metallicRoughnessMap), texCoord).gb;
        roughness = metallicRoughness.x;
        roughness = roughness * roughness;
        metallic = metallicRoughness.y; 
    }
    outMaterial.roughness = roughness;
    outMaterial.metallic = metallic;

    outMaterial.occlusion = 1.f;
    if ((desc.flag & kHasOcclusionMap) != 0u) {
        outMaterial.occlusion = texture(sampler2D(desc.occlusionMap), texCoord).r;
		outMaterial.occlusion = pow(outMaterial.occlusion, 3.0f);
    }
	return outMaterial;
}
//===============================================================================

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
    float tanThetaV2 = (1.f - ndotv * ndotv) / max((ndotv * ndotv), 1e-2);
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
float LambertBRDF() {
    return 1.f / pi;
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
    return D * F * G / max((4.f * ndotv * ndotl), 1e-1);
}

/**
    f0 is specular reflectance
*/
vec3 calcF0(in Material material) {
    return mix(vec3(0.04f), material.albedo, material.metallic);
}

vec3 calcDirectionalLight(in DirectionalLight directionalLight, in Material material, vec3 worldSpacePosition) {
    vec3 radiance = vec3(0.f);
    // view direction in world space
    vec3 worldSpaceViewDirection = (inverse(viewSsbo.view) * vec4(normalize(-psIn.viewSpacePosition), 0.f)).xyz;
    float ndotl = max(dot(material.normal, directionalLight.direction.xyz), 0.f);
    vec3 f0 = calcF0(material);
    vec3 li = directionalLight.colorAndIntensity.rgb * directionalLight.colorAndIntensity.a;

    /** 
    * diffuse
    */
    vec3 diffuse = mix(material.albedo, vec3(0.f), material.metallic) * LambertBRDF();

    /** 
    * specular
    */
    vec3 specular = CookTorranceBRDF(directionalLight.direction.xyz, worldSpaceViewDirection, material.normal, material.roughness, f0);

    radiance += (diffuse + specular) * li * ndotl;

    // shadow
    radiance *= calcDirectionalShadow(worldSpacePosition, material.normal, directionalLight);
    return radiance;
}

vec3 calcPointLight()
{
    vec3 radiance = vec3(0.f);
    return radiance;
}

uniform uint64_t ssaoTextureHandle;
uniform float ssaoEnabled;
uniform uint64_t ssbnTextureHandle;
uniform float ssbnEnabled;
uniform vec2 outputSize;

vec3 calcSkyLight(SkyLight inSkyLight, in Material material, vec3 worldSpacePosition) {
    vec3 radiance = vec3(0.f);

    vec3 worldSpaceViewDirection = (inverse(viewSsbo.view) * vec4(normalize(-psIn.viewSpacePosition), 0.0)).xyz;
    float ndotv = saturate(dot(material.normal, worldSpaceViewDirection));

    vec3 f0 = calcF0(material);

    float ao = 1.f;
	vec2 texCoord = gl_FragCoord.xy / outputSize;
    if (ssaoEnabled > .5f)
    {
        ao = texture(sampler2D(ssaoTextureHandle), texCoord).r;
    }

    // irradiance
    vec3 diffuse = mix(material.albedo, vec3(0.f), material.metallic);
    vec3 irradiance = diffuse * texture(skyLight.irradiance, material.normal).rgb; 
    if (ssbnEnabled > .5f)
    {
        vec3 bentNormal = normalize(texture(sampler2D(ssbnTextureHandle), texCoord).rgb * 2.f - 1.f);
		irradiance = diffuse * texture(skyLight.irradiance, bentNormal).rgb; 
    }
    else
    {
		irradiance = diffuse * texture(skyLight.irradiance, material.normal).rgb; 
    }
    radiance += irradiance * ao;

    // reflection
    vec3 reflectionDirection = -reflect(worldSpaceViewDirection, material.normal);
    vec3 BRDF = texture(sampler2D(inSkyLight.BRDFLookupTexture), vec2(ndotv, material.roughness)).rgb; 
    vec3 incidentRadiance = textureLod(samplerCube(skyLight.reflection), reflectionDirection, material.roughness * 10.f).rgb;
    radiance += incidentRadiance * (f0 * BRDF.r + BRDF.g);

    return radiance;
}

vec3 calcLighting(in Material material, vec3 worldSpacePosition) {
    vec3 radiance = vec3(0.f);

    // sun light
    radiance += calcDirectionalLight(directionalLights[0], material, worldSpacePosition);
    // sky light
    radiance += calcSkyLight(skyLight, material, worldSpacePosition);

    return radiance;
}

void main() 
{
    vec3 worldSpaceTangent = normalize(psIn.worldSpaceTangent);
    vec3 worldSpaceNormal = normalize(psIn.worldSpaceNormal);
    worldSpaceTangent = normalize(worldSpaceTangent - dot(worldSpaceNormal, worldSpaceTangent) * worldSpaceNormal); 
    vec3 worldSpaceBitangent = normalize(cross(worldSpaceNormal, worldSpaceTangent)) * psIn.tangentSpaceHandedness;

    Material material = getMaterial(psIn.desc, worldSpaceNormal, worldSpaceTangent, worldSpaceBitangent, psIn.texCoord0);
    outColor = calcLighting(material, psIn.worldSpacePosition);
}
