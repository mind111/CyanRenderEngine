#version 450 core

layout(location = 0) out vec3 outAlbedo;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outMetallicRoughness;

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
} psIn;

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

uniform sampler2D mp_heightMapTex;
uniform sampler2D mp_normalMapTex;

Material calcMaterialProperties(vec3 worldSpaceNormal, vec3 worldSpaceTangent, vec3 worldSpaceBitangent, vec2 texCoord) 
{
    Material outMaterial;
    outMaterial.albedo = texture(mp_heightMapTex, texCoord).rgb;
    outMaterial.normal = texture(mp_normalMapTex, texCoord).rgb;
    outMaterial.metallic = 0.f;
    outMaterial.roughness = .5f;
	return outMaterial;
}

void main()
{
    vec3 worldSpaceTangent = normalize(psIn.worldSpaceTangent);
    vec3 worldSpaceNormal = normalize(psIn.worldSpaceNormal);
    worldSpaceTangent = normalize(worldSpaceTangent - dot(worldSpaceNormal, worldSpaceTangent) * worldSpaceNormal); 
    vec3 worldSpaceBitangent = normalize(cross(worldSpaceNormal, worldSpaceTangent)) * psIn.tangentSpaceHandedness;

    Material material = calcMaterialProperties(worldSpaceNormal, worldSpaceTangent, worldSpaceBitangent, psIn.texCoord0);

    outAlbedo = material.albedo;
    outNormal = material.normal * .5f + .5f;
    outMetallicRoughness = vec3(material.metallic, material.roughness, 0.f);

    outAlbedo = vec3(.2f, .5, .9f);
    outNormal = psIn.worldSpaceNormal * .5f + .5f;
    outMetallicRoughness = vec3(0.f, .5f, 0.f);
}
