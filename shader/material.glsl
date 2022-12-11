#extension GL_NV_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable 

/**
* Shared material definitions
*/
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

/**
	material converted from application side material definition 
*/
struct Material {
	vec3 albedo;
	vec3 normal;
	float roughness;
	float metallic;
	float occlusion;
};

vec4 tangentSpaceToViewSpace(vec3 tn, vec3 vn, vec3 t) {
    mat4 tbn;
    // apply Gram-Schmidt process
    t = normalize(t - dot(t, vn) * vn);
    vec3 b = cross(vn, t);
    tbn[0] = vec4(t, 0.f);
    tbn[1] = vec4(b, 0.f);
    tbn[2] = vec4(vn, 0.f);
    tbn[3] = vec4(0.f, 0.f, 0.f, 1.f);
    return tbn * vec4(tn, 0.0f);
}

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
