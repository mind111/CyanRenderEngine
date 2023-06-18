#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outColor;

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
uniform mat4 csmPrimaryViewViewMatrix;
uniform mat4 csmPrimaryViewProjectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform sampler2D sceneDepth;
uniform sampler2D sceneColor;

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) 
{
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
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

void main() 
{
	outColor = texture(sceneColor, psIn.texCoord0).rgb;

	float depth = texture(sceneDepth, psIn.texCoord0).r;
    // todo: need a more reliable way to distinguish pixels that doesn't overlap any geometry,
    // maybe use stencil buffer to mark?
	if (depth <= 0.999999f) 
	{
		vec3 worldSpacePosition = screenToWorld(vec3(psIn.texCoord0, depth) * 2.f - 1.f, inverse(viewMatrix), inverse(projectionMatrix)); 
		vec3 shadowViewSpacePosition = (directionalLight.csm.lightViewMatrix * vec4(worldSpacePosition, 1.f)).xyz;
		vec3 csmPrimaryViewSpacePosition = (csmPrimaryViewViewMatrix * vec4(worldSpacePosition, 1.f)).xyz;

		if (csmPrimaryViewSpacePosition.z <= 0.f)
		{
			int cascadeIndex = calcCascadeIndex(csmPrimaryViewSpacePosition, directionalLight); 

			vec4 shadowClipCoord = (directionalLight.csm.cascades[cascadeIndex].lightProjectionMatrix * vec4(shadowViewSpacePosition, 1.f));
			shadowClipCoord /= shadowClipCoord.w;
			vec2 shadowCoord = shadowClipCoord.xy * .5 + .5f;

			if (shadowCoord.x >= 0.f && shadowCoord.x <= 1.f && shadowCoord.y >= 0.f && shadowCoord.y <= 1.f)
			{
				float t = float(cascadeIndex) / kNumShadowCascades;
				vec3 green = vec3(0.f, 1.f, 0.f);
				vec3 red = vec3(1.f, 0.f, 0.f);
				const float discretization = 128.f;
				float texelSize = 1.f / discretization;
				vec2 shadowTexelCenter = (floor(shadowCoord * discretization) + .5f) / discretization;
				float d = length(shadowCoord - shadowTexelCenter);
				vec3 tint = vec3((floor(shadowCoord * discretization) + .5f) / discretization, t);
				tint = mix(tint, vec3(.2), d / (.5 * texelSize));
				outColor = mix(texture(sceneColor, psIn.texCoord0).rgb, tint, .99);
			}
		}
	}
}
