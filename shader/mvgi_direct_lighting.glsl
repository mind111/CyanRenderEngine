#version 450

#extension GL_NV_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable 

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outColor;

const uint kNumShadowCascades = 4;
struct DirectionalShadowMap 
{
	mat4 lightSpaceView;
	mat4 lightSpaceProjection;
	uint64_t depthTextureHandle;
	vec2 padding;
};

struct Cascade 
{
	float n;
	float f;
	vec2 padding;
	DirectionalShadowMap shadowMap;
};

struct CascadedShadowMap 
{
	Cascade cascades[kNumShadowCascades];
};

struct DirectionalLight 
{
	vec4 colorAndIntensity;
	vec4 direction;
	CascadedShadowMap csm;
};

layout(std430) buffer ViewBuffer 
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
};

layout (std430) buffer DirectionalLightBuffer 
{
	DirectionalLight directionalLights[];
};

uniform sampler2D mvgiPosition;
uniform sampler2D mvgiNormal;
uniform sampler2D mvgiAlbedo;

float constantBias()
{
    return 0.0025f;
}

/**
	determine which cascade to sample from
*/
int calcCascadeIndex(in vec3 viewSpacePosition, in DirectionalLight directionalLight)
{
    int cascadeIndex = 0;
    for (int i = 0; i < 4; ++i)
    {
        if (viewSpacePosition.z < directionalLight.csm.cascades[i].f)
        {
            cascadeIndex = i;
            break;
        }
    }
    return cascadeIndex;
}

float PCFShadow(vec3 worldSpacePosition, vec3 normal, in DirectionalLight directionalLight)
{
    sampler2D sampler = sampler2D(directionalLight.csm.cascades[0].shadowMap.depthTextureHandle);
	float shadow = 0.0f;
    vec2 texelOffset = vec2(1.f) / textureSize(sampler, 0);
    vec3 viewSpacePosition = (view * vec4(worldSpacePosition, 1.f)).xyz;
    int cascadeIndex = calcCascadeIndex(viewSpacePosition, directionalLight);
    vec4 lightSpacePosition = 
		directionalLight.csm.cascades[cascadeIndex].shadowMap.lightSpaceProjection 
	  * directionalLight.csm.cascades[cascadeIndex].shadowMap.lightSpaceView * vec4(worldSpacePosition, 1.f);
    float depth = lightSpacePosition.z * .5f + .5f;
    vec2 uv = lightSpacePosition.xy * .5f + .5f;

    const int kernelRadius = 2;
    // 5 x 5 filter kernel
    float kernel[25] = {
        0.04, 0.04, 0.04, 0.04, 0.04,
        0.04, 0.04, 0.04, 0.04, 0.04,
        0.04, 0.04, 0.04, 0.04, 0.04,
        0.04, 0.04, 0.04, 0.04, 0.04,
        0.04, 0.04, 0.04, 0.04, 0.04
    };

    for (int i = -kernelRadius; i <= kernelRadius; ++i)
    {
        for (int j = -kernelRadius; j <= kernelRadius; ++j)
        {
            vec2 offset = vec2(i, j) * texelOffset;
            vec2 texCoord = uv + offset;
            if (texCoord.x < 0.f || texCoord.x > 1.f || texCoord.y < 0.f || texCoord.y > 1.f) 
            {
                continue;
			}
			float bias = constantBias();
            float shadowSample = texture(sampler, texCoord).r < (depth - bias) ? 0.f : 1.f;
            shadow += shadowSample * kernel[(i + kernelRadius) * 5 + (j + kernelRadius)];
        }
    }
    return shadow;
}

float calcDirectionalShadow(vec3 worldPosition, vec3 normal, in DirectionalLight directionalLight)
{
    return PCFShadow(worldPosition, normal, directionalLight);
}

void main() 
{
	vec3 position = texture(mvgiPosition, psIn.texCoord0).xyz;
	vec3 normal = normalize(texture(mvgiNormal, psIn.texCoord0).xyz * 2.f - 1.f);
	vec3 albedo = texture(mvgiAlbedo, psIn.texCoord0).rgb;
	float ndotl = max(dot(normal, directionalLights[0].direction.xyz), 0.f);
	vec3 li = directionalLights[0].colorAndIntensity.rgb * directionalLights[0].colorAndIntensity.w;
    float shadow = calcDirectionalShadow(position, normal, directionalLights[0]);
	outColor = li * ndotl * albedo * shadow;
}
