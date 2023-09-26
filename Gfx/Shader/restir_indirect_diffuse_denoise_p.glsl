#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outColor;

uniform sampler2D u_indirectIrradianceTex;
uniform sampler2D u_sceneDepthTex;
uniform sampler2D u_sceneNormalTex;
uniform float u_kernel[25];
uniform vec2 u_offsets[25];
uniform int u_passIndex;
uniform uvec2 u_renderResolution;  
uniform float u_normalEdgeStopping;  
uniform float u_sigmaN;
uniform float u_positionEdgeStopping;  
uniform float u_sigmaP;

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

vec3 screenToWorld(in vec2 pixelCoord, in sampler2D depthTex, mat4 viewMatrix, mat4 projectionMatrix)
{
	float depth = texture(depthTex, pixelCoord).r;
	vec3 NDCSpacePos = vec3(pixelCoord, depth) * 2.f - 1.f;
    vec4 clipSpacePos = inverse(projectionMatrix) * vec4(NDCSpacePos, 1.f);
    vec3 viewSpacePos = clipSpacePos.xyz / clipSpacePos.w;
    vec3 worldSpacePos = (inverse(viewMatrix) * vec4(viewSpacePos, 1.f)).xyz;
    return worldSpacePos;
}

bool isValidScreenCoord(vec2 coord)
{
	return (coord.x >= 0.f && coord.x <= 1.f && coord.y >= 0.f && coord.y <= 1.f); 
}

// todo: non edge stopping version of A-Trous
void main()
{
	float depth = texture(u_sceneDepthTex, psIn.texCoord0).r;
	if (depth > .999999f)
	{
		discard;
	}

    vec3 p = screenToWorld(psIn.texCoord0, u_sceneDepthTex, viewParameters.viewMatrix, viewParameters.projectionMatrix);
	vec3 n = texture(u_sceneNormalTex, psIn.texCoord0).rgb * 2.f - 1.f;
	vec3 c = texture(u_indirectIrradianceTex, psIn.texCoord0).rgb;

	vec2 texelSize = 1.f / u_renderResolution;
	float stepSize = pow(2.f, u_passIndex);

	vec3 filteredColor = vec3(0.f);
	float wSum = 0.f;
	for (int i = 0; i < 25; ++i)
	{
		vec2 sampleCoord = psIn.texCoord0 + u_offsets[i] * stepSize * texelSize;
		if (!isValidScreenCoord(sampleCoord))
		{
			continue;
		}
		float weight = u_kernel[i]; 

		vec3 t;
		float dist2;

		vec3 neighborP = screenToWorld(sampleCoord, u_sceneDepthTex, viewParameters.viewMatrix, viewParameters.projectionMatrix);
		t = neighborP - p;
		dist2 = dot(t, t);
		float wp = min(exp(-dist2 / (u_sigmaP * u_sigmaP)), 1.f);
		if (u_positionEdgeStopping > .5f)
		{
			weight *= wp;
		}

		vec3 neighborN = texture(u_sceneNormalTex, sampleCoord).rgb * 2.f - 1.f;
		t = neighborN - n;
		dist2 = max(dot(t, t) / (stepSize * stepSize), 0.f);
		float wn = min(exp(-dist2 / (u_sigmaN * u_sigmaN)), 1.f);
		if (u_normalEdgeStopping > .5f)
		{
			weight *= wn;
		}

		vec3 neighborColor = texture(u_indirectIrradianceTex, sampleCoord).rgb;
		filteredColor += weight * neighborColor;
		wSum += weight;
	}
	filteredColor /= wSum;
	outColor = filteredColor;
}
