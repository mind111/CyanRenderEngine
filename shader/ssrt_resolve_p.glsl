#version 450 core
#define PI 3.1415926

in VSOutput
{
	vec2 texCoord0;
} psIn;

layout(location = 2) out vec3 outIrradiance;

uniform sampler2D depthBuffer;
uniform sampler2D normalBuffer;
uniform sampler2DArray hitPositionBuffer;
uniform sampler2DArray hitNormalBuffer;
uniform sampler2DArray hitRadianceBuffer;

layout(std430) buffer ViewBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
};

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) 
{
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
}

vec3 worldToScreen(vec3 pp, in mat4 view, in mat4 projection)
{
    vec4 p = projection * view * vec4(pp, 1.f);
    p /= p.w;
    return p.xyz * .5f + .5f;
}

void main()
{
	vec3 textureDim = textureSize(hitPositionBuffer, 0);
	int numSamples = int(textureDim.z);
	vec3 irradiance = vec3(0.f);

	float depth = texture(depthBuffer, psIn.texCoord0).r;

	if (depth > 0.9999f)
	{
		discard;
	}
	vec3 worldSpaceRO = screenToWorld(vec3(psIn.texCoord0, depth) * 2.f - 1.f, inverse(view), inverse(projection));
	vec3 worldSpaceNormal = normalize(texture(normalBuffer, psIn.texCoord0).rgb * 2.f - 1.f);

	// no reuse first
	for (int i = 0; i < numSamples; ++i)
	{
		vec3 texCoord = vec3(psIn.texCoord0, i);

		vec4 hitPosition = texture(hitPositionBuffer, texCoord).rgba;
		if (hitPosition.w > 0.f)
		{
			vec3 hitNormal = texture(hitNormalBuffer, texCoord).rgb;
			vec3 hitRadiance = texture(hitRadianceBuffer, texCoord).rgb;
			vec3 rd = normalize(hitPosition.xyz - worldSpaceRO);
			bool bInUpperHemisphere = dot(hitNormal, -rd) > 0.f;
			if (bInUpperHemisphere)
			{
				irradiance += hitRadiance * max(dot(worldSpaceNormal, rd), 0.f);
			}
		}
	}

	outIrradiance = irradiance / float(numSamples);
}
