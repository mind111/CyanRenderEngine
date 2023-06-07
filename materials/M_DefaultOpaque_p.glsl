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

uniform sampler2D mp_albedoMap;
uniform sampler2D mp_normalMap;
uniform sampler2D mp_metallicRoughnessMap;
uniform sampler2D mp_emissiveMap;
uniform sampler2D mp_occlusionMap;
uniform vec3 mp_albedo;
uniform float mp_metallic;
uniform float mp_roughness;
uniform float mp_emissive;
uniform uint mp_flag;

const uint kHasAlbedoMap            = 1 << 0;
const uint kHasNormalMap            = 1 << 1;
const uint kHasMetallicRoughnessMap = 1 << 2;
const uint kHasOcclusionMap         = 1 << 3;

Material calcMaterialProperties(vec3 worldSpaceNormal, vec3 worldSpaceTangent, vec3 worldSpaceBitangent, vec2 texCoord) 
{
    Material outMaterial;
    
    outMaterial.normal = worldSpaceNormal;
    if ((mp_flag & kHasNormalMap) != 0u) 
    {
        vec3 tangentSpaceNormal = texture(mp_normalMap, texCoord).xyz;
        // Convert from [0, 1] to [-1.0, 1.0] and renomalize if texture filtering changes the length
        tangentSpaceNormal = normalize(tangentSpaceNormal * 2.f - 1.f);
        // Covert normal from tangent frame to camera space
        outMaterial.normal = normalize(tangentSpaceToWorldSpace(worldSpaceTangent, worldSpaceBitangent, worldSpaceNormal, tangentSpaceNormal).xyz);
    }

    outMaterial.albedo = mp_albedo.rgb;
    if ((mp_flag & kHasAlbedoMap) != 0u) 
    {
        outMaterial.albedo = texture(mp_albedoMap, texCoord).rgb;
		// from sRGB to linear space if using a texture
		outMaterial.albedo = vec3(pow(outMaterial.albedo.r, 2.2f), pow(outMaterial.albedo.g, 2.2f), pow(outMaterial.albedo.b, 2.2f));
    }

    // According to gltf-2.0 spec, metal is sampled from b, roughness is sampled from g
    float roughness = mp_roughness, metallic = mp_metallic;
    if ((mp_flag & kHasMetallicRoughnessMap) != 0u)
    {
        vec2 metallicRoughness = texture(mp_metallicRoughnessMap, texCoord).gb;
        roughness = metallicRoughness.x;
        // roughness = roughness * roughness;
        metallic = metallicRoughness.y; 
    }
    outMaterial.roughness = roughness;
    outMaterial.metallic = metallic;

    outMaterial.occlusion = 1.f;
    if ((mp_flag & kHasOcclusionMap) != 0u) 
    {
        outMaterial.occlusion = texture(mp_occlusionMap, texCoord).r;
		outMaterial.occlusion = pow(outMaterial.occlusion, 3.0f);
    }
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
}