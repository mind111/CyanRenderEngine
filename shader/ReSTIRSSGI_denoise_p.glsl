#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outIndirectIrradiance;

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
uniform sampler2D sceneDepthBuffer;
uniform sampler2D indirectIrradiance;

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

// todo: improve range weighting (Gr)
// todo: seperable filter
void main()
{
	vec2 texCoord = psIn.texCoord0;
	float deviceDepth = texture(sceneDepthBuffer, texCoord).r;
	vec3 viewSpacePosition = (viewParameters.viewMatrix * vec4(screenToWorld(vec3(texCoord, deviceDepth) * 2.f - 1.f, inverse(viewParameters.viewMatrix), inverse(viewParameters.projectionMatrix)), 1.f)).xyz;
	float linearDepth = -viewSpacePosition.z;
	vec2 texelSize = 1.f / textureSize(indirectIrradiance, 0).xy;

#if 1
#define NUM_TAPS 5
	// these weights are calculated using http://demofox.org/gauss.html 
	const float gaussianWeights[NUM_TAPS * NUM_TAPS] = {
		0.0038,	0.0150,	0.0238,	0.0150,	0.0038,
		0.0150,	0.0599,	0.0949,	0.0599,	0.0150,
		0.0238,	0.0949,	0.1503,	0.0949,	0.0238,
		0.0150,	0.0599,	0.0949,	0.0599,	0.0150,
		0.0038,	0.0150,	0.0238,	0.0150,	0.0038,
	};
#else
#define NUM_TAPS 7
	// these weights are calculated using http://demofox.org/gauss.html 
	const float gaussianWeights[NUM_TAPS * NUM_TAPS] = {
		0.0000,	0.0004,	0.0014,	0.0023,	0.0014,	0.0004,	0.0000,
		0.0004,	0.0037,	0.0147,	0.0232,	0.0147,	0.0037,	0.0004,
		0.0014,	0.0147,	0.0585,	0.0927,	0.0585,	0.0147,	0.0014,
		0.0023,	0.0232,	0.0927,	0.1468,	0.0927,	0.0232,	0.0023,
		0.0014,	0.0147,	0.0585,	0.0927,	0.0585,	0.0147,	0.0014,
		0.0004,	0.0037,	0.0147,	0.0232,	0.0147,	0.0037,	0.0004,
		0.0000,	0.0004,	0.0014,	0.0023,	0.0014,	0.0004,	0.0000
	};
#endif

	float weightSum = 0.f;
	vec3 filteredIndirectIrradiance = vec3(0.f);

	// NUM_TAPS x NUM_TAPS bilateral filtering
	for (int i = 0; i < NUM_TAPS; ++i)
	{
		for (int j = 0; j < NUM_TAPS; ++j)
		{
			vec2 offset = vec2(i - 3, j - 3) * texelSize;
			vec2 sampleCoord = texCoord + offset;
			if (abs(sampleCoord.x * 2.f - 1.f) <= 1.f && abs(sampleCoord.y * 2.f - 1.f) <= 1.f)
			{
				vec3 irradianceTap = texture(indirectIrradiance, sampleCoord).rgb;
				float depthTap = texture(sceneDepthBuffer, sampleCoord).r;
				float linearDepthTap = -(viewParameters.viewMatrix * vec4(screenToWorld(vec3(sampleCoord, depthTap) * 2.f - 1.f, inverse(viewParameters.viewMatrix), inverse(viewParameters.projectionMatrix)), 1.f)).z;
				float depthDelta = abs(linearDepthTap - linearDepth);
				float Gr = clamp(-1.f * depthDelta + linearDepth * .1f, 0.f, 1.f);
				float w = gaussianWeights[i * NUM_TAPS + j] * Gr;

				filteredIndirectIrradiance += w * irradianceTap;
				weightSum += w;
			}
		}
	}

	filteredIndirectIrradiance /= weightSum;
	outIndirectIrradiance = filteredIndirectIrradiance;
}
