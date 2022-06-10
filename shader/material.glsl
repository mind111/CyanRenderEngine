
/**
* Shared material definitions
*/
uniform struct MaterialParameter_PBR
{
	int M_flags;
	sampler2D M_albedo;
	sampler2D M_normal;
	sampler2D M_roughness;
	sampler2D M_metallic;
	sampler2D M_metallicRoughness;
	sampler2D M_occlusion;
	float M_kRoughness;
	float M_kMetallic;
	vec3 M_kAlbedo;
};
