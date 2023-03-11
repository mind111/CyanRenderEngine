#version 450 core

#extension GL_NV_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable 

#define pi 3.14159265359
in VSOutput
{
	vec2 texCoord0;
} psIn;

layout (location = 0) out vec3 outRadiance;
layout (location = 1) out vec3 outDiffuse;

uniform sampler2D sceneDepth;
uniform sampler2D sceneNormal;
uniform sampler2D sceneAlbedo;
uniform sampler2D sceneMetallicRoughness;

const uint kNumShadowCascades = 4;
struct Cascade 
{
	float n;
	float f;
	uint64_t depthTextureHandle;
    mat4 lightSpaceProjection;
};

struct DirectionalShadowMap 
{
    mat4 lightSpaceView;
	Cascade cascades[kNumShadowCascades];
};

struct DirectionalLight 
{
	vec4 colorAndIntensity;
	vec4 direction;
	DirectionalShadowMap shadowMap;
};

layout (std430) buffer DirectionalLightBuffer 
{
	DirectionalLight directionalLight;
};

layout(std430) buffer ViewBuffer 
{
    mat4  view;
    mat4  projection;
};

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) 
{
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
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

struct Material 
{
	vec3 albedo;
	vec3 normal;
	float roughness;
	float metallic;
	float occlusion;
};

/**
    f0 is specular reflectance
*/
vec3 calcF0(in Material material) {
    return mix(vec3(0.04f), material.albedo, material.metallic);
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
        if (viewSpacePosition.z < directionalLight.shadowMap.cascades[i].f)
        {
            cascadeIndex = i;
            break;
        }
    }
    return cascadeIndex;
}

float PCFShadow(vec3 worldSpacePosition, vec3 normal, in DirectionalLight directionalLight)
{
    sampler2D sampler = sampler2D(directionalLight.shadowMap.cascades[0].depthTextureHandle);
	float shadow = 0.0f;
    vec2 texelOffset = vec2(1.f) / textureSize(sampler, 0);
    vec3 viewSpacePosition = (view * vec4(worldSpacePosition, 1.f)).xyz;
    int cascadeIndex = calcCascadeIndex(viewSpacePosition, directionalLight);
    vec4 lightSpacePosition = 
		directionalLight.shadowMap.cascades[cascadeIndex].lightSpaceProjection 
	  * directionalLight.shadowMap.lightSpaceView * vec4(worldSpacePosition, 1.f);
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

vec3 calcDirectionalLight(in DirectionalLight directionalLight, in Material material, vec3 worldSpacePosition, inout vec3 outDiffuseRadiance) 
{
    vec3 radiance = vec3(0.f);
    // view direction in world space
    vec3 viewSpacePosition = (view * vec4(worldSpacePosition, 1.f)).xyz;
    vec3 worldSpaceViewDirection = (inverse(view) * vec4(normalize(-viewSpacePosition), 0.f)).xyz;
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
    float shadow = calcDirectionalShadow(worldSpacePosition, material.normal, directionalLight);

    radiance *= shadow;
    outDiffuseRadiance += diffuse * li * ndotl * shadow;
    return radiance;
}

vec3 calcDirectLighting(in Material material, vec3 worldSpacePosition) 
{
    vec3 radiance = vec3(0.f);
    // sun light
    radiance += calcDirectionalLight(directionalLight, material, worldSpacePosition, outDiffuse);
    return radiance;
}

void main()
{
	float depth = texture(sceneDepth, psIn.texCoord0).r;
	if (depth > 0.999) 
	{
		discard;
	}
	vec3 normal = normalize(texture(sceneNormal, psIn.texCoord0).rgb * 2.f - 1.f);
	vec3 worldSpacePosition = screenToWorld(vec3(psIn.texCoord0, depth) * 2.f - 1.f, inverse(view), inverse(projection)); 

    // setup per pixel material 
    Material material;
    material.albedo = texture(sceneAlbedo, psIn.texCoord0).rgb;
    material.normal = normal;
    vec2 metallicRoughness = texture(sceneMetallicRoughness, psIn.texCoord0).rg;
    material.metallic = metallicRoughness.r; 
    material.roughness = metallicRoughness.g; 
    material.occlusion = 1.f;

    outRadiance = calcDirectLighting(material, worldSpacePosition);
}
