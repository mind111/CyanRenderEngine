#version 450 core

layout (location = 0) out vec4 outColor; 

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

void main()
{
	// flip the normal if it's a backface
	vec3 viewDirection = normalize(viewParameters.cameraPosition -  psIn.worldSpacePosition);
	bool backfacing = (sign(dot(viewDirection, psIn.worldSpaceNormal)) <= 0.f);
	vec3 n = backfacing ? psIn.worldSpaceNormal * -1.f : psIn.worldSpaceNormal;
	float ndotl = dot(n, normalize(vec3(0.f, 1.f, 1.f))) * .5f + .5f;
	vec3 color = vec3(0.f, 1.f, 0.f) * ndotl;
	color *= backfacing ? .1f : 1.f;
	outColor = vec4(color, .9f);
}
