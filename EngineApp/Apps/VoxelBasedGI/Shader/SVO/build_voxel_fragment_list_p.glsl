#version 450 core

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

struct VoxelFragment
{
    vec4 position; 
    vec4 albedo;
    vec4 normal;
    vec4 directLighting;
};

layout (binding = 0) uniform atomic_uint u_voxelFragmentCounter;

layout (std430) buffer VoxelFragmentBuffer {
    VoxelFragment voxelFragmentList[];
};

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
    uint fragmentIndex = atomicCounterIncrement(u_voxelFragmentCounter);
	VoxelFragment fragment;
	fragment.position = vec4(psIn.position, 1.f);
    vec3 diffuseAlbedo = psIn.albedo * (1.f - psIn.metallic);
    // note that albedo is storing only diffuse albedo
	fragment.albedo = vec4(diffuseAlbedo, 1.f);
	fragment.normal = vec4(psIn.normal, 1.f);
    // perform forward lighting here and store direct radiance
    float shadow = calcDirectionalShadow(worldSpacePosition, psIn.normal, directionalLight);
    float ndotl = max(dot(psIn.normal, directionalLight.direction.xyz), 0.f);
    vec3 li = directionalLight.color * directionalLight.intensity;
    // should divided by PI or not
    vec3 diffuseRadiance = diffuseAlbedo * li * ndotl * shadow;
    fragment.directLighting = vec4(diffuseRadiance, 1.f);

	voxelFragmentList[fragmentIndex] = fragment;
}
