#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outColor;

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
uniform sampler2D u_sceneDepthTex;
uniform sampler2D u_sceneNormalTex;
uniform sampler3D u_voxelGridIrradianceTex;
uniform vec3 u_voxelGridCenter;
uniform vec3 u_voxelGridExtent;

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
	float depth = texture(u_sceneDepthTex, psIn.texCoord0).r;
	if (depth > .999999f) 
	{
		discard;
	}
	vec3 n = normalize(texture(u_sceneNormalTex, psIn.texCoord0).rgb * 2.f - 1.f);
	mat4 view = viewParameters.viewMatrix, projection = viewParameters.projectionMatrix;
	vec3 worldSpacePosition = screenToWorld(vec3(psIn.texCoord0, depth) * 2.f - 1.f, inverse(view), inverse(projection));

	vec3 voxelGridOrigin = u_voxelGridCenter + vec3(-u_voxelGridExtent.x, -u_voxelGridExtent.y, u_voxelGridExtent.z);
	vec3 v = worldSpacePosition - voxelGridOrigin;
	v.z *= -1.f;
	vec3 uv = v / (u_voxelGridExtent * 2.f);
	vec3 irradiance = texture(u_voxelGridIrradianceTex, uv).rgb;
	outColor = irradiance;
}
