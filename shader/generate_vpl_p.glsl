#version 450 core

#extension GL_NV_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable 

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

layout(std430, binding = 0) buffer ViewShaderStorageBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

struct VPL {
	vec4 position;
	vec4 normal;
	vec4 flux;
};

layout (binding = 0, offset = 0) uniform atomic_uint numGeneratedVPLs;
layout(std430, binding = 47) buffer VPLSSBO
{
	VPL VPLs[];
};

struct DirectionalShadowmap
{
	mat4 lightSpaceProjection;
	uint64_t depthTextureHandle;
} directionalShadowmap;

struct Cascade
{
	float n;
	float f;
	DirectionalShadowmap shadowmap;
} cascade;

#define NUM_SHADOW_CASCADES 4
struct CascadedShadowmap
{
	Cascade cascades[NUM_SHADOW_CASCADES];
} csm;

struct DirectionalLight
{
	vec4 direction;
	vec4 colorAndIntensity;
    mat4 lightSpaceView;
	CascadedShadowmap csm;
} directionalLight;

uniform struct PointLight
{
	vec3 position;
	vec4 colorAndIntensity;
	samplerCube shadowmap;
} pointLight;

uniform struct SkyLight
{
	samplerCube irradiance;
	samplerCube reflection;
} skyLightDef;

uniform struct SceneLights
{
    uint64_t BRDFLookupTexture;
	SkyLight skyLight;
	DirectionalLight directionalLight;
	// PointLight pointLights[MAX_NUM_POINT_LIGHTS];
} sceneLights;

/* note: 
* rand number generator taken from https://www.shadertoy.com/view/4lfcDr
*/
uint flat_idx;
uint seed;
void encrypt_tea(inout uvec2 arg)
{
	uvec4 key = uvec4(0xa341316c, 0xc8013ea4, 0xad90777d, 0x7e95761e);
	uint v0 = arg[0], v1 = arg[1];
	uint sum = 0u;
	uint delta = 0x9e3779b9u;

	for(int i = 0; i < 32; i++) {
		sum += delta;
		v0 += ((v1 << 4) + key[0]) ^ (v1 + sum) ^ ((v1 >> 5) + key[1]);
		v1 += ((v0 << 4) + key[2]) ^ (v0 + sum) ^ ((v0 >> 5) + key[3]);
	}
	arg[0] = v0;
	arg[1] = v1;
}

vec2 get_random()
{
  	uvec2 arg = uvec2(flat_idx, seed++);
  	encrypt_tea(arg);
  	return fract(vec2(arg) / vec2(0xffffffffu));
} 

/**
	determine which cascade to sample from
*/
int calcCascadeIndex(in vec3 viewSpacePosition) {
    int cascadeIndex = 0;
    for (int i = 0; i < 4; ++i)
    {
        if (viewSpacePosition.z < sceneLights.directionalLight.csm.cascades[i].f)
        {
            cascadeIndex = i;
            break;
        }
    }
    return cascadeIndex;
}

bool isDirectlyLit(vec3 worldSpacePosition, in DirectionalLight directionalLight) {
    sampler2D sampler = sampler2D(directionalLight.csm.cascades[0].shadowmap.depthTextureHandle);
    vec3 viewSpacePosition = (viewSsbo.view * vec4(worldSpacePosition, 1.f)).xyz;
    int cascadeIndex = calcCascadeIndex(viewSpacePosition);
    vec4 lightSpacePosition = directionalLight.csm.cascades[cascadeIndex].shadowmap.lightSpaceProjection * directionalLight.lightSpaceView * vec4(worldSpacePosition, 1.f);
    float depth = lightSpacePosition.z * .5f + .5f;
    vec2 uv = lightSpacePosition.xy * .5f + .5f;
	return texture(sampler, uv).r > (depth - 0.0025f);
}

void main() {
	// initialize random number generator state
	seed = 0;
	flat_idx = uint(floor(gl_FragCoord.y) * 2560 + floor(gl_FragCoord.x));

	// do shadow test to see if current shaded surface point is directly visible to the light source
	if (isDirectlyLit(psIn.worldSpacePosition, sceneLights.directionalLight)) {
		vec2 rng = get_random();
		if (rng.x < 0.001f) { 
			uint index = atomicCounterIncrement(numGeneratedVPLs);
			if (index < 256) {
				VPLs[index].position = vec4(psIn.worldSpacePosition, 1.f);
				VPLs[index].normal = vec4(1.f);
				VPLs[index].flux = vec4(1.f);
			}
		}
	}
}
