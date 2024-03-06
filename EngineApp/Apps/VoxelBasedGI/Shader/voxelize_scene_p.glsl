#version 450 core
#extension GL_NV_shader_atomic_float : require 

#define PI 3.1415926

in VertexData
{
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 texCoord;
    vec3 albedo;
    float roughness;
    float metallic;
} psIn;

// opacity
layout(binding = 0, r8) uniform image3D voxelGridOpacityTex;
// normal
layout(binding = 1, r32f) uniform image3D voxelGridNormalX;
layout(binding = 2, r32f) uniform image3D voxelGridNormalY;
layout(binding = 3, r32f) uniform image3D voxelGridNormalZ;
// radiance
layout(binding = 4, r32f) uniform image3D u_voxelGridRadianceR;
layout(binding = 5, r32f) uniform image3D u_voxelGridRadianceG;
layout(binding = 6, r32f) uniform image3D u_voxelGridRadianceB;
// counter
layout(binding = 7, r32f) uniform image3D u_voxelGridFragmentCounter;

uniform vec3 u_voxelGridCenter;
uniform vec3 u_voxelGridExtent;
uniform vec3 u_voxelGridResolution;

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

void main()
{
    vec3 worldSpacePosition = psIn.position;
    vec3 p = u_voxelGridCenter + vec3(-u_voxelGridExtent.x, -u_voxelGridExtent.y, u_voxelGridExtent.z);
    vec3 uv = (worldSpacePosition - p) / (u_voxelGridExtent * 2.f);
    uv.z *= -1.f;
    ivec3 coord = ivec3(floor(uv * u_voxelGridResolution));

    // 1 for opaque
    imageStore(voxelGridOpacityTex, coord, vec4(1u));

    // store normal
    imageAtomicAdd(voxelGridNormalX, coord, psIn.normal.x);
    imageAtomicAdd(voxelGridNormalY, coord, psIn.normal.y);
    imageAtomicAdd(voxelGridNormalZ, coord, psIn.normal.z);

    // perform forward lighting here and store direct radiance
    float shadow = calcDirectionalShadow(worldSpacePosition, psIn.normal, directionalLight);
    vec3 viewDirection = normalize(viewParameters.cameraPosition - worldSpacePosition);
    float ndotl = max(dot(psIn.normal, directionalLight.direction.xyz), 0.f);
    vec3 li = directionalLight.color * directionalLight.intensity;
    vec3 diffuseAlbedo = psIn.albedo * (1.f - psIn.metallic);
    // should divided by PI or not
    vec3 diffuseRadiance = diffuseAlbedo * li * ndotl * shadow;

    imageAtomicAdd(u_voxelGridRadianceR, coord, diffuseRadiance.r);
    imageAtomicAdd(u_voxelGridRadianceG, coord, diffuseRadiance.g);
    imageAtomicAdd(u_voxelGridRadianceB, coord, diffuseRadiance.b);

    // increment the counter
    imageAtomicAdd(u_voxelGridFragmentCounter, coord, 1.f);
}
