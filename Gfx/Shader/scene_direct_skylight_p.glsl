#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outColor;

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

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) 
{
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
}

uniform ViewParameters viewParameters;
uniform sampler2D u_sceneDepth;
uniform sampler2D u_sceneNormal;
uniform sampler2D u_sceneAlbedo;
uniform sampler2D u_sceneMetallicRoughness;
uniform samplerCube u_irradianceCubemap;
uniform samplerCube u_reflectionCubemap;
uniform sampler2D u_BRDFLUT;
uniform sampler2D u_SSAOTex;

struct Material 
{
	vec3 albedo;
	vec3 worldSpacePosition;
	vec3 worldSpaceNormal;
	float roughness;
	float metallic;
	float occlusion;
};

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

vec3 calcIrradiance(Material material)
{
	vec3 irradiance = vec3(0.f);
    vec3 diffuseColor = (1.f - material.metallic) * material.albedo;
    irradiance = diffuseColor * texture(u_irradianceCubemap, material.worldSpaceNormal).rgb; 
	return irradiance;
}

vec3 calcReflection(vec3 worldSpaceViewDirection, Material material)
{
	vec3 reflection = vec3(0.f);
    vec3 f0 = calcF0(material);
    vec3 reflectionDirection = -reflect(worldSpaceViewDirection, material.worldSpaceNormal);
    vec3 incidentRadiance = textureLod(u_reflectionCubemap, reflectionDirection, material.roughness * log2(textureSize(u_reflectionCubemap, 0).x)).rgb;
    float ndotv = clamp(dot(material.worldSpaceNormal, worldSpaceViewDirection), 0.f, 1.f);
    vec3 BRDF = texture(u_BRDFLUT, vec2(ndotv, material.roughness)).rgb; 
    reflection = incidentRadiance * (f0 * BRDF.r + BRDF.g);
	return reflection;
}

void main()
{
	outColor = vec3(psIn.texCoord0, 1.f);

	float depth = texture(u_sceneDepth, psIn.texCoord0).r;
	if (depth >= 0.999999f) discard;

    // setup per pixel material 
    Material material;
    material.albedo = texture(u_sceneAlbedo, psIn.texCoord0).rgb;
	material.worldSpacePosition = screenToWorld(vec3(psIn.texCoord0, depth) * 2.f - 1.f, inverse(viewParameters.viewMatrix), inverse(viewParameters.projectionMatrix));
    material.worldSpaceNormal = normalize(texture(u_sceneNormal, psIn.texCoord0).rgb * 2.f - 1.f);
    vec3 MRO = texture(u_sceneMetallicRoughness, psIn.texCoord0).rgb;
    material.metallic = MRO.r; 
    material.roughness = MRO.g; 
    material.occlusion = MRO.b;

	vec3 irradiance = calcIrradiance(material);
	vec3 worldSpaceViewDirection = normalize(material.worldSpacePosition - viewParameters.cameraPosition);
	vec3 reflection = calcReflection(worldSpaceViewDirection, material);

	float SSAO = texture(u_SSAOTex, psIn.texCoord0).r;

	outColor = (irradiance + reflection) * material.occlusion * SSAO;
}
