#version 450 core

in VSOutput
{
	vec3 viewSpacePosition;
	vec3 worldSpacePosition;
	vec3 worldSpaceNormal;
	vec3 viewSpaceTangent;
	vec2 texCoord0;
	vec2 texCoord1;
} psIn;

out vec3 outColor;

const uint kHasRoughnessMap         = 1 << 0;
const uint kHasMetallicMap          = 1 << 1;
const uint kHasMetallicRoughnessMap = 1 << 2;
const uint kHasOcclusionMap         = 1 << 3;
const uint kHasDiffuseMap           = 1 << 4;
const uint kHasNormalMap            = 1 << 5;
const uint kUsePrototypeTexture     = 1 << 6;
const uint kUseLightMap             = 1 << 7;            

/**
	mirror's pbr material definition on application side
*/
uniform struct MaterialInput
{
	uint M_flags;
	sampler2D M_albedo;
	sampler2D M_normal;
	sampler2D M_roughness;
	sampler2D M_metallic;
	sampler2D M_metallicRoughness;
	sampler2D M_occlusion;
	float M_kRoughness;
	float M_kMetallic;
	vec3 M_kAlbedo;
} materialInput;

/**
	material parameters processed and converted from application side material definition 
*/
struct MaterialParameters
{
	vec3 albedo;
	vec3 normal;
	float roughness;
	float metallic;
	float occlusion;
};

vec4 tangentSpaceToViewSpace(vec3 tn, vec3 vn, vec3 t) 
{
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

MaterialParameters getMaterialParameters(vec3 worldSpaceNormal, vec3 worldSpaceTangent, vec2 texCoord)
{
	MaterialParameters materialParameters;

    if ((materialInput.M_flags & kHasNormalMap) != 0u)
    {
        vec3 tn = texture(materialInput.M_normal, texCoord).xyz;
        // Convert from [0, 1] to [-1.0, 1.0] and renomalize if texture filtering changes the length
        tn = normalize(tn * 2.f - vec3(1.f));
        // Covert normal from tangent frame to camera space
        materialParameters.normal = tangentSpaceToViewSpace(tn, worldSpaceNormal, worldSpaceTangent).xyz;
    }

    materialParameters.albedo = materialInput.M_kAlbedo;
    if ((materialInput.M_flags & kHasDiffuseMap) != 0u)
    {
        materialParameters.albedo = texture(materialInput.M_albedo, texCoord).rgb;
		// from sRGB to linear space if using a texture
		materialParameters.albedo = vec3(pow(materialParameters.albedo.r, 2.2f), pow(materialParameters.albedo.g, 2.2f), pow(materialParameters.albedo.b, 2.2f));
    }

    // According to gltf-2.0 spec, metal is sampled from b, roughness is sampled from g
    float roughness = materialInput.M_kRoughness, metallic = materialInput.M_kMetallic;
    if ((materialInput.M_flags & kHasMetallicRoughnessMap) != 0u)
    {
        vec2 metallicRoughness = texture(materialInput.M_metallicRoughness, texCoord).gb;
        roughness = metallicRoughness.x;
        roughness = roughness * roughness;
        metallic = metallicRoughness.y; 
    }
    else if ((materialInput.M_flags & kHasRoughnessMap) != 0u)
    {
        roughness = texture(materialInput.M_roughness, texCoord).r;
        roughness = roughness * roughness;
        metallic = materialInput.M_kMetallic;
    } 
    else
    {
        roughness = materialInput.M_kRoughness;
        metallic = materialInput.M_kMetallic;
    }
    materialParameters.roughness = roughness;
    materialParameters.metallic = metallic;

    materialParameters.occlusion = 1.f;
    if ((materialInput.M_flags & kHasOcclusionMap) != 0)
    {
        materialParameters.occlusion = texture(materialInput.M_occlusion, texCoord).r;
		materialParameters.occlusion = pow(materialParameters.occlusion, 3.0f);
    }

	return materialParameters;
}

struct DirectionalLight
{
	vec3 direction;
	vec4 colorAndIntensity;
    // mat4 lightSpaceView;
	// CascadedShadowmap csm// ;
};

vec3 calcDirectionalLight(DirectionalLight directionalLight, MaterialParameters materialParameters, vec3 worldSpacePosition)
{
    vec3 radiance = vec3(0.f);
    float ndotl = max(dot(materialParameters.normal, directionalLight.direction), 0.f);
    radiance += materialParameters.albedo * ndotl;
    return radiance;
}
void main()
{
    vec3 viewSpaceTangent = normalize(psIn.viewSpaceTangent);
    vec3 worldSpaceNormal = normalize(psIn.worldSpaceNormal);
    // todo: transform tangent back to world space
    MaterialParameters materialParameters = getMaterialParameters(worldSpaceNormal, viewSpaceTangent, psIn.texCoord0);
    DirectionalLight directionalLight = DirectionalLight(vec3(1.f), vec4(1.f));
	outColor = calcDirectionalLight(directionalLight, materialParameters, psIn.worldSpacePosition);
}
