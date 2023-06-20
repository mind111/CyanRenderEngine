#version 450 core

#extension GL_NV_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable 

#define pi 3.14159265359

uniform sampler2D sceneDepth;
uniform sampler2D sceneNormal;
uniform sampler2D sceneAlbedo;
uniform sampler2D sceneMetallicRoughness;

uniform float SSGIAOEnabled;
uniform sampler2D SSGIAO;

uniform float SSGIDiffuseEnabled;
uniform sampler2D SSGIDiffuse;

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outRadiance;

struct ViewParameters
{
	uvec2 renderResolution;
	float aspectRatio;
	mat4 viewMatrix;
	mat4 projectionMatrix;
	vec3 cameraPosition;
	vec3 cameraRight;
	vec3 cameraForward;
	vec3 cameraUp;
	int frameCount;
	float elapsedTime;
	float deltaTime;
};

uniform ViewParameters viewParameters;

struct SkyLight 
{
	float intensity;
	samplerCube irradiance;
	samplerCube reflection;
    sampler2D BRDFLookupTexture;
}; 

uniform SkyLight skyLight;

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

float GGX(float roughness, float ndoth) 
{
    float alpha = roughness;
    float alpha2 = alpha * alpha;
    float result = max(alpha2, 1e-6); // prevent the nominator goes to 0 when roughness equals 0
    float denom = ndoth * ndoth * (alpha2 - 1.f) + 1.f;
    result /= (pi * denom * denom); 
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

/**
 * The 4 * notl * ndotv in the original G_SmithGGX is cancelled with thet
    4 * ndotl * ndotv term in the denominator of Cook-Torrance BRDF, forming this 
    simplified V term. Thus when using this V term, the BRDF becomes D * F * V without
    needing to divide by 4 * ndotl * ndotv.
 */
float V_SmithGGXHeightCorrelated(float ndotv, float ndotl, float roughness)
{
    float a2 = roughness * roughness;
    float v = ndotl * sqrt(ndotv * ndotv * (1.0 - a2) + a2);
    float l = ndotv * sqrt(ndotl * ndotl * (1.0 - a2) + a2);
    return 0.5f / (v + l);
}

float LambertBRDF() 
{
    return 1.f / pi;
}

vec3 CookTorranceBRDF(vec3 wi, vec3 wo, vec3 n, float roughness, vec3 f0)
{
    float remappedRoughness = roughness * roughness;

    float ndotv = saturate(dot(n, wo)) + 1e-5;
    float ndotl = saturate(dot(n, wi));
    vec3 h = normalize(wi + wo);
    float ndoth = saturate(dot(n, h));
    float vdoth = saturate(dot(wo, h));

    float D = GGX(remappedRoughness, ndoth);
    float V = V_SmithGGXHeightCorrelated(ndotv, ndotl, remappedRoughness);
    vec3 F = fresnel(f0, vdoth);
    return D * F * V;
}

/**
    f0 is specular reflectance (or specular color)
*/
vec3 calcF0(in Material material) 
{
	// todo: dialectric formula should be .16 * specular * specular
    vec3 dialectricF0 = vec3(0.16) * (.5f * .5f);
    vec3 conductorF0 = material.albedo;
    return mix(dialectricF0, conductorF0, material.metallic);
}

float calcAO()
{
    float ao = 1.f;
	vec2 texCoord = psIn.texCoord0;
    if (SSGIAOEnabled > .5f)
    {
        ao = texture(SSGIAO, texCoord).r;
    }
    return ao;
}

vec3 calcIndirectIrradiance()
{
    vec3 indirectIrradiance = vec3(0.f);
    if (SSGIDiffuseEnabled > .5f)
    {
        indirectIrradiance = texture(SSGIDiffuse, psIn.texCoord0).rgb;
    }
    return indirectIrradiance;
}

vec3 calcSkyLight(SkyLight inSkyLight, in Material material, vec3 worldSpacePosition, in float ao) 
{
    vec3 radiance = vec3(0.f);
    vec3 viewSpacePosition = (viewParameters.viewMatrix * vec4(worldSpacePosition, 1.f)).xyz;
    vec3 worldSpaceViewDirection = (inverse(viewParameters.viewMatrix) * vec4(normalize(-viewSpacePosition), 0.0)).xyz;
    float ndotv = saturate(dot(material.normal, worldSpaceViewDirection));

    vec3 f0 = calcF0(material);
    // irradiance
    vec3 diffuseColor = (1.f - material.metallic) * material.albedo;
    vec3 skyIrradiance = diffuseColor * texture(skyLight.irradiance, material.normal).rgb; 
    radiance += skyIrradiance * ao;

    // reflection
    vec3 reflectionDirection = -reflect(worldSpaceViewDirection, material.normal);
    vec3 incidentRadiance = textureLod(samplerCube(skyLight.reflection), reflectionDirection, material.roughness * log2(textureSize(skyLight.reflection, 0).x)).rgb;
    vec3 BRDF = texture(inSkyLight.BRDFLookupTexture, vec2(ndotv, material.roughness)).rgb; 
    radiance += incidentRadiance * (f0 * BRDF.r + BRDF.g) * ao;

    return radiance;
}


void main() 
{
    outRadiance = vec3(0.f);

	float depth = texture(sceneDepth, psIn.texCoord0).r;
    // todo: need a more reliable way to distinguish pixels that doesn't overlap any geometry,
    // maybe use stencil buffer to mark?
	if (depth >= 0.999999f) 
	{
		discard;
	}
	vec3 normal = normalize(texture(sceneNormal, psIn.texCoord0).rgb * 2.f - 1.f);
	vec3 worldSpacePosition = screenToWorld(vec3(psIn.texCoord0, depth) * 2.f - 1.f, inverse(viewParameters.viewMatrix), inverse(viewParameters.projectionMatrix)); 

    // setup per pixel material 
    Material material;
    material.albedo = texture(sceneAlbedo, psIn.texCoord0).rgb;
    material.normal = normalize(texture(sceneNormal, psIn.texCoord0).rgb * 2.f - 1.f);
    vec2 metallicRoughness = texture(sceneMetallicRoughness, psIn.texCoord0).rg;
    material.metallic = metallicRoughness.r; 
    material.roughness = metallicRoughness.g; 
    material.occlusion = 1.f;

    float ao = calcAO();
    outRadiance += calcSkyLight(skyLight, material, worldSpacePosition, ao);
    vec3 indirectIrradiance = calcIndirectIrradiance();
    vec3 diffuseColor = (1.f - material.metallic) * material.albedo;
    outRadiance += diffuseColor * indirectIrradiance * ao;
}
