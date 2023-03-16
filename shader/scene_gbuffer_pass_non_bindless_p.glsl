#version 450 core

layout(location = 0) out vec3 outAlbedo;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outMetallicRoughness;

struct MaterialDesc
{
	sampler2D albedoMap;
	sampler2D normalMap;
	sampler2D metallicRoughnessMap;
	sampler2D emissiveMap;
    sampler2D occlusionMap;
	sampler2D padding;
	vec4 albedo;
    float metallic;
    float roughness;
    float emissive;
    uint flag;
};

uniform MaterialDesc materialDesc;

const uint kHasAlbedoMap            = 1 << 0;
const uint kHasNormalMap            = 1 << 1;
const uint kHasMetallicRoughnessMap = 1 << 2;
const uint kHasOcclusionMap         = 1 << 3;

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

Material getMaterial(in MaterialDesc desc, vec3 worldSpaceNormal, vec3 worldSpaceTangent, vec3 worldSpaceBitangent, vec2 texCoord) 
{
    Material outMaterial;
    
    outMaterial.normal = worldSpaceNormal;
    if ((desc.flag & kHasNormalMap) != 0u) 
    {
        vec3 tangentSpaceNormal = texture(desc.normalMap, texCoord).xyz;
        // Convert from [0, 1] to [-1.0, 1.0] and renomalize if texture filtering changes the length
        tangentSpaceNormal = normalize(tangentSpaceNormal * 2.f - 1.f);
        // Covert normal from tangent frame to camera space
        outMaterial.normal = normalize(tangentSpaceToWorldSpace(worldSpaceTangent, worldSpaceBitangent, worldSpaceNormal, tangentSpaceNormal).xyz);
    }

    outMaterial.albedo = desc.albedo.rgb;
    if ((desc.flag & kHasAlbedoMap) != 0u) 
    {
        outMaterial.albedo = texture(desc.albedoMap, texCoord).rgb;
		// from sRGB to linear space if using a texture
		outMaterial.albedo = vec3(pow(outMaterial.albedo.r, 2.2f), pow(outMaterial.albedo.g, 2.2f), pow(outMaterial.albedo.b, 2.2f));
    }

    // According to gltf-2.0 spec, metal is sampled from b, roughness is sampled from g
    float roughness = desc.roughness, metallic = desc.metallic;
    if ((desc.flag & kHasMetallicRoughnessMap) != 0u)
    {
        vec2 metallicRoughness = texture(desc.metallicRoughnessMap, texCoord).gb;
        roughness = metallicRoughness.x;
        // roughness = roughness * roughness;
        metallic = metallicRoughness.y; 
    }
    outMaterial.roughness = roughness;
    outMaterial.metallic = metallic;

    outMaterial.occlusion = 1.f;
    if ((desc.flag & kHasOcclusionMap) != 0u) 
    {
        outMaterial.occlusion = texture(desc.occlusionMap, texCoord).r;
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
#if 1
    Material material = getMaterial(materialDesc, worldSpaceNormal, worldSpaceTangent, worldSpaceBitangent, psIn.texCoord0);

    outAlbedo = material.albedo;
    outNormal = material.normal * .5f + .5f;
    outMetallicRoughness = vec3(material.metallic, material.roughness, 0.f);
#else
    outAlbedo = vec3(1.f);
    outNormal = worldSpaceNormal * .5f + .5f;
    outMetallicRoughness = vec3(0.f, .5f, 0.f);
#endif
}
