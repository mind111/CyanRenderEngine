#version 450 core

#define pi 3.14159265359

uniform sampler2D sceneDepth;
uniform sampler2D sceneNormal;
uniform sampler2D sceneAlbedo;
uniform sampler2D sceneMetallicRoughness;

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

struct Settings 
{
	int kTracingStopMipLevel;
	int kMaxNumIterationsPerRay;
    float bNearFieldSSAO;
	float bSSDO;
    float bIndirectIrradiance;
	float indirectBoost;
};
uniform Settings settings;

uniform sampler2D SSGI_NearFieldSSAO;
uniform sampler2D SSGI_Diffuse;

float calcAO()
{
    float ao = 1.f;
	vec2 texCoord = psIn.texCoord0;
    if (settings.bNearFieldSSAO > .5f)
    {
        ao = texture(SSGI_NearFieldSSAO, texCoord).r;
    }
    return ao;
}

vec3 calcIndirectIrradiance()
{
    vec3 indirectIrradiance = vec3(0.f);
    if (settings.bIndirectIrradiance > .5f)
    {
        indirectIrradiance = texture(SSGI_Diffuse, psIn.texCoord0).rgb;
        indirectIrradiance *= settings.indirectBoost;
    }
    return indirectIrradiance;
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
    vec3 indirectIrradiance = calcIndirectIrradiance();
    vec3 diffuseColor = (1.f - material.metallic) * material.albedo;
    outRadiance += diffuseColor * indirectIrradiance * ao;
}
