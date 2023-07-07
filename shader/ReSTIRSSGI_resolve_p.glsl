#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outIndirectIrradiance;

uniform sampler2D sceneDepthBuffer;
uniform sampler2D sceneNormalBuffer;
uniform sampler2D reservoirRadiance; 
uniform sampler2D reservoirPosition; 
uniform sampler2D reservoirNormal; 
uniform sampler2D reservoirWSumMW;

struct ViewParameters
{
	uvec2 renderResolution;
	float aspectRatio;
	mat4 viewMatrix;
    mat4 prevFrameViewMatrix;
	mat4 projectionMatrix;
    mat4 prevFrameProjectionMatrix;
	vec3 cameraPosition;
	vec3 prevFrameCameraPosition;
	vec3 cameraRight;
	vec3 cameraForward;
	vec3 cameraUp;
	int frameCount;
	float elapsedTime;
	float deltaTime;
};
uniform ViewParameters viewParameters;

struct Reservoir
{
    vec3 radiance;
    vec3 position;
    vec3 normal;
    float wSum;
    float M;
    float W;
};

Reservoir getReservoir(vec2 sampleCoord)
{
    Reservoir r;
    r.radiance = texture(reservoirRadiance, sampleCoord).rgb;
    r.position = texture(reservoirPosition, sampleCoord).xyz;
    r.normal = texture(reservoirNormal, sampleCoord).xyz * 2.f - 1.f;
    vec3 wSumMW = texture(reservoirWSumMW, sampleCoord).xyz;
    r.wSum = wSumMW.x;
    r.M = wSumMW.y;
    r.W = wSumMW.z;
    return r;
}

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) 
{
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
}

void main()
{
    Reservoir r = getReservoir(psIn.texCoord0);

	float deviceDepth = texture(sceneDepthBuffer, psIn.texCoord0).r; 
	vec3 n = texture(sceneNormalBuffer, psIn.texCoord0).rgb * 2.f - 1.f;

    if (deviceDepth > 0.999999f) discard;

    vec3 worldSpacePosition = screenToWorld(vec3(psIn.texCoord0, deviceDepth) * 2.f - 1.f, inverse(viewParameters.viewMatrix), inverse(viewParameters.projectionMatrix));

	vec3 l = normalize(r.position - worldSpacePosition);
	if (dot(r.normal, -l) > 0.f)
	{
		float ndotl = max(dot(n, l), 0.f);
		outIndirectIrradiance = r.radiance * ndotl * r.W;
	}
	else
	{
		outIndirectIrradiance = vec3(0.f);
	}
}
