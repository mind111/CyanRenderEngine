#version 450 core

#extension GL_NV_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable 

#define pi 3.14159265359

uniform sampler2D sceneDepth;
uniform sampler2D sceneNormal;
uniform sampler2D sceneAlbedo;
uniform sampler2D sceneMetallicRoughness;

uniform float ssaoEnabled;
uniform sampler2D ssao;
uniform float ssbnEnabled;
uniform sampler2D ssbn;
uniform sampler2D indirectIrradiance;
uniform float indirectBoost;

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outRadiance;

layout(std430) buffer ViewBuffer 
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
};

uniform struct SkyLight 
{
	float intensity;
	samplerCube irradiance;
	samplerCube reflection;
    sampler2D BRDFLookupTexture;
} skyLight;

struct Material 
{
	vec3 albedo;
	vec3 normal;
	float roughness;
	float metallic;
	float occlusion;
};

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) 
{
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
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

vec3 calcSkyLight(SkyLight inSkyLight, in Material material, vec3 worldSpacePosition) {
    vec3 radiance = vec3(0.f);
    vec3 viewSpacePosition = (view * vec4(worldSpacePosition, 1.f)).xyz;
    vec3 worldSpaceViewDirection = (inverse(view) * vec4(normalize(-viewSpacePosition), 0.0)).xyz;
    float ndotv = saturate(dot(material.normal, worldSpaceViewDirection));

    vec3 f0 = calcF0(material);

    float ao = 1.f;
	vec2 texCoord = psIn.texCoord0;
    if (ssaoEnabled > .5f)
    {
        ao = texture(ssao, texCoord).r;
    }

    // irradiance
    vec3 diffuse = mix(material.albedo, vec3(0.04f), material.metallic);
    vec3 irradiance = diffuse * texture(skyLight.irradiance, material.normal).rgb; 
    if (ssbnEnabled > .5f)
    {
        vec3 bentNormal = normalize(texture(ssbn, texCoord).rgb * 2.f - 1.f);
		irradiance = diffuse * texture(skyLight.irradiance, bentNormal).rgb; 
    }
    else
    {
		irradiance = diffuse * texture(skyLight.irradiance, material.normal).rgb; 
    }
    radiance += irradiance * ao;

    // todo: improve the specular part
    // reflection
    vec3 reflectionDirection = -reflect(worldSpaceViewDirection, material.normal);
    vec3 BRDF = texture(inSkyLight.BRDFLookupTexture, vec2(ndotv, material.roughness)).rgb; 
    vec3 incidentRadiance = textureLod(samplerCube(skyLight.reflection), reflectionDirection, material.roughness * log2(textureSize(skyLight.reflection, 0).x)).rgb;
    radiance += incidentRadiance * (f0 * BRDF.r + BRDF.g);

    return radiance;
}

uniform float indirectIrradianceEnabled;

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
    material.normal = normalize(texture(sceneNormal, psIn.texCoord0).rgb * 2.f - 1.f);
    vec2 metallicRoughness = texture(sceneMetallicRoughness, psIn.texCoord0).rg;
    material.metallic = metallicRoughness.r; 
    material.roughness = metallicRoughness.g; 
    material.occlusion = 1.f;
    
    outRadiance = calcSkyLight(skyLight, material, worldSpacePosition);
    if (indirectIrradianceEnabled > 0.f)
    {
		vec3 diffuse = mix(material.albedo, vec3(0.04f), material.metallic);
		outRadiance += diffuse * texture(indirectIrradiance, psIn.texCoord0).rgb;
	}
}
