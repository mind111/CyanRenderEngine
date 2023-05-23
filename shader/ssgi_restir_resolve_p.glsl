#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

uniform sampler2D reservoirRadiance; 
uniform sampler2D reservoirSamplePosition; 
uniform sampler2D reservoirSampleNormal; 
uniform sampler2D reservoirWSumMW;

struct Reservoir
{
    vec3 sampleRadiance;
    vec3 samplePosition;
    vec3 sampleNormal;
    float wSum;
    float M;
    float W;
};

layout(std430) buffer ViewBuffer
{
    mat4  view;
    mat4  projection;
};

Reservoir getReservoir(vec2 sampleCoord)
{
    Reservoir r;
    r.sampleRadiance = texture(reservoirRadiance, sampleCoord).rgb;
    r.samplePosition = texture(reservoirSamplePosition, sampleCoord).xyz;
    r.sampleNormal = texture(reservoirSampleNormal, sampleCoord).xyz * 2.f - 1.f;
    vec3 wSumMW = texture(reservoirWSumMW, sampleCoord).xyz;
    r.wSum = wSumMW.x;
    r.M = wSumMW.y;
    r.W = wSumMW.z;
    return r;
}

uniform sampler2D sceneDepthBuffer;
uniform sampler2D sceneNormalBuffer;

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) 
{
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
}

out vec3 outIndirectIrradiance;

void main()
{
    Reservoir r = getReservoir(psIn.texCoord0);

	float deviceDepth = texture(sceneDepthBuffer, psIn.texCoord0).r; 
	vec3 n = texture(sceneNormalBuffer, psIn.texCoord0).rgb * 2.f - 1.f;

    if (deviceDepth > 0.998f) discard;

    vec3 worldSpacePosition = screenToWorld(vec3(psIn.texCoord0, deviceDepth) * 2.f - 1.f, inverse(view), inverse(projection));

	vec3 l = normalize(r.samplePosition - worldSpacePosition);
	if (dot(r.sampleNormal, -l) > 0.f)
	{
		float ndotl = max(dot(n, l), 0.f);
		outIndirectIrradiance = r.sampleRadiance * ndotl * r.W;
	}
	else
	{
		outIndirectIrradiance = vec3(0.f);
	}
}
