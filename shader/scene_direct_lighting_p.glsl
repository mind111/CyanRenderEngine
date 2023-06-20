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
layout (location = 2) out vec3 outLightingOnly;

uniform sampler2D sceneDepth;
uniform sampler2D sceneNormal;
uniform sampler2D sceneAlbedo;
uniform sampler2D sceneMetallicRoughness;

const uint kNumShadowCascades = 4;
struct Cascade 
{
	float n;
	float f;
    mat4 lightProjectionMatrix;
	sampler2D depthTexture;
};

struct CascadedShadowMap 
{
    mat4 lightViewMatrix;
	Cascade cascades[kNumShadowCascades];
};

struct DirectionalLight 
{
	vec3 color;
    float intensity;
	vec3 direction;
	CascadedShadowMap csm;
};

uniform DirectionalLight directionalLight;

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
    return 0.5f / max((v + l), 1e-5);
}

float LambertBRDF() 
{
    return 1.f / pi;
}

vec3 CookTorranceBRDF(vec3 wi, vec3 wo, vec3 n, float roughness, vec3 f0)
{
    float remappedRoughness = roughness * roughness;

    float ndotv = saturate(dot(n, wo));
    float ndotl = saturate(dot(n, wi));
    vec3 h = normalize(wi + wo);
    float ndoth = saturate(dot(n, h));
    float vdoth = saturate(dot(wo, h));

    float D = GGX(remappedRoughness, ndoth);
    float V = V_SmithGGXHeightCorrelated(ndotv, ndotl, remappedRoughness);
    vec3 F = fresnel(f0, vdoth);
    return D * F * V;
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
vec3 calcF0(in Material material) 
{
    vec3 dialectricF0 = vec3(.16f) * (.5f) * (.5f);
    vec3 conductorF0 = material.albedo;
    return mix(dialectricF0, conductorF0, material.metallic);
}

float constantBias()
{
    return 0.0030f;
}

/**
	determine which cascade to sample from
*/
int calcCascadeIndex(in vec3 viewSpacePosition, in DirectionalLight directionalLight)
{
    int cascadeIndex = 0;
    for (int i = 0; i < 4; ++i)
    {
        if (abs(viewSpacePosition.z) < directionalLight.csm.cascades[i].f)
        {
            cascadeIndex = i;
            break;
        }
    }
    return cascadeIndex;
}

float calcDirectionalShadow(vec3 worldSpacePosition, vec3 normal, in DirectionalLight directionalLight)
{
	float shadow = 1.0f;
    vec3 viewSpacePosition = (viewParameters.viewMatrix * vec4(worldSpacePosition, 1.f)).xyz;
    int cascadeIndex = calcCascadeIndex(viewSpacePosition, directionalLight);

    vec4 shadowSpacePosition = directionalLight.csm.cascades[cascadeIndex].lightProjectionMatrix * directionalLight.csm.lightViewMatrix * vec4(worldSpacePosition, 1.f);
    float shadowSpaceZ = shadowSpacePosition.z * .5f + .5f;
    vec2 shadowCoord = shadowSpacePosition.xy * .5f + .5f;
	if (shadowCoord.x >= 0.f && shadowCoord.x <= 1.f && shadowCoord.y >= 0.f && shadowCoord.y <= 1.f) 
	{
		float bias = constantBias();
		shadow = texture(directionalLight.csm.cascades[cascadeIndex].depthTexture, shadowCoord).r < (shadowSpaceZ - bias) ? 0.f : 1.f;
	}
    return shadow;
}

vec3 calcDirectionalLight(in DirectionalLight directionalLight, in Material material, vec3 worldSpacePosition, inout vec3 diffuseOnly) 
{
    vec3 radiance = vec3(0.f);

    // shadow
    float shadow = calcDirectionalShadow(worldSpacePosition, material.normal, directionalLight);

    // view direction in world space
    vec3 viewSpacePosition = (viewParameters.viewMatrix * vec4(worldSpacePosition, 1.f)).xyz;
    vec3 worldSpaceViewDirection = (inverse(viewParameters.viewMatrix) * vec4(normalize(-viewSpacePosition), 0.f)).xyz;
    float ndotl = max(dot(material.normal, directionalLight.direction.xyz), 0.f);
    vec3 li = directionalLight.color * directionalLight.intensity;

    vec3 diffuseColor = material.albedo * (1.f - material.metallic);
    vec3 diffuse = diffuseColor * LambertBRDF();
    vec3 f0 = calcF0(material); // f0 is specular color
    vec3 specular = CookTorranceBRDF(directionalLight.direction.xyz, worldSpaceViewDirection, material.normal, material.roughness, f0);

    radiance += (diffuse + specular) * li * ndotl * shadow;
    diffuseOnly += diffuse * li * ndotl * shadow;
    return radiance;
}

vec3 calcDirectLighting(in Material material, vec3 worldSpacePosition, out vec3 diffuseOnly) 
{
    vec3 radiance = vec3(0.f);
    // sun light
    radiance += calcDirectionalLight(directionalLight, material, worldSpacePosition, diffuseOnly);
    return radiance;
}

vec3 calcDirectLightingOnly(in Material material, vec3 worldSpacePosition)
{
    vec3 outDirectLightingOnly = vec3(0.f);

    material.albedo = vec3(1.f);

    // view direction in world space
    vec3 viewSpacePosition = (viewParameters.viewMatrix * vec4(worldSpacePosition, 1.f)).xyz;
    vec3 worldSpaceViewDirection = (inverse(viewParameters.viewMatrix) * vec4(normalize(-viewSpacePosition), 0.f)).xyz;
    float ndotl = max(dot(material.normal, directionalLight.direction.xyz), 0.f);
    vec3 f0 = calcF0(material);
    vec3 li = directionalLight.color * directionalLight.intensity;

    /** 
    * diffuse
    */
    vec3 diffuse = mix(material.albedo, vec3(0.f), material.metallic) * LambertBRDF();

    /** 
    * specular
    */
    vec3 specular = CookTorranceBRDF(directionalLight.direction.xyz, worldSpaceViewDirection, material.normal, material.roughness, f0);

    outDirectLightingOnly += (diffuse + specular) * li * ndotl;

    // shadow
    float shadow = calcDirectionalShadow(worldSpacePosition, material.normal, directionalLight);

    outDirectLightingOnly *= shadow;
    outDirectLightingOnly += diffuse * li * ndotl * shadow;

    return outDirectLightingOnly;
}

void main()
{
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
    material.normal = normal;
    vec2 metallicRoughness = texture(sceneMetallicRoughness, psIn.texCoord0).rg;
    material.metallic = metallicRoughness.r; 
    material.roughness = metallicRoughness.g; 
    material.occlusion = 1.f;

    outRadiance = calcDirectLighting(material, worldSpacePosition, outDiffuse);
    outLightingOnly = calcDirectLightingOnly(material, worldSpacePosition);
}
