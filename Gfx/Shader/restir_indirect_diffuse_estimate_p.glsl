#version 450 core

#define PI 3.1415926

in VSOutput
{
	vec2 texCoord0;
} psIn;

layout (location = 0) out vec3 outIndirectIrradiance;
layout (location = 1) out vec3 outIndirectLighting;

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

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) 
{
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
}

uniform ViewParameters viewParameters;
uniform sampler2D u_sceneDepthTex; 
uniform sampler2D u_sceneNormalTex; 
uniform sampler2D u_sceneAlbedoTex; 
uniform sampler2D u_sceneMetallicRoughnessTex; 
uniform sampler2D u_reservoirRadiance;
uniform sampler2D u_reservoirPosition;
uniform sampler2D u_reservoirNormal;
uniform sampler2D u_reservoirWSumMW;

#define SKY_DIST 1e5

void main()
{
	float depth = texture(u_sceneDepthTex, psIn.texCoord0).r; 

    if (depth > 0.999999f) discard;

	vec3 worldSpacePosition = screenToWorld(vec3(psIn.texCoord0, depth) * 2.f - 1.f, inverse(viewParameters.viewMatrix), inverse(viewParameters.projectionMatrix)); 
	vec3 n = texture(u_sceneNormalTex, psIn.texCoord0).rgb * 2.f - 1.f;
	vec3 albedo = texture(u_sceneAlbedoTex, psIn.texCoord0).rgb;
	vec3 MRO = texture(u_sceneMetallicRoughnessTex, psIn.texCoord0).rgb;
	float metallic = MRO.r;
	float roughness = MRO.g;

	vec3 reservoirRadiance = texture(u_reservoirRadiance, psIn.texCoord0).rgb;
	vec3 reservoirPosition = texture(u_reservoirPosition, psIn.texCoord0).rgb;
	vec3 reservoirNormal = texture(u_reservoirNormal, psIn.texCoord0).rgb * 2.f - 1.f;
	vec3 reservoirWSumMW = texture(u_reservoirWSumMW, psIn.texCoord0).rgb;

    vec3 indirectIrradianceEstimate = vec3(0.f);
	vec3 rd = normalize(reservoirPosition - worldSpacePosition);
	// todo: this term introduce lots of noise!!! so leaving it out at the moment
	if (dot(-rd, reservoirNormal) > 0.f)
	{
		float ndotl = max(dot(rd, n), 0.f);
		indirectIrradianceEstimate = reservoirRadiance * reservoirWSumMW.z * ndotl;
	}

	outIndirectIrradiance = indirectIrradianceEstimate;
	vec3 diffuseColor = (1.f - metallic) * albedo;
	outIndirectLighting = diffuseColor * indirectIrradianceEstimate;
}
