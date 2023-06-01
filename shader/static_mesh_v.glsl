#version 450 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 tangent;
layout (location = 3) in vec2 texCoord0;
layout (location = 4) in vec2 texCoord1;

// this is necessary since using seperable program
out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out VertexShaderOutput 
{
	vec3 viewSpacePosition;
	vec3 worldSpacePosition;
	vec3 worldSpaceNormal;
	vec3 worldSpaceTangent;
	flat float tangentSpaceHandedness;
	vec2 texCoord0;
	vec2 texCoord1;
	vec3 vertexColor;
} vsOut;

struct ViewParameters
{
	uvec2 renderResolution;
	float aspectRatio;
	mat4 viewMatrix;
	mat4 projectionMatrix;
	vec3 cameraPosition;
	vec3 cameraLookAt;
	vec3 cameraRight;
	vec3 cameraForward;
	vec3 cameraUp;
	int frameCount;
	float elapsedTime;
	float deltaTime;
};

uniform mat4 localToWorld;
uniform ViewParameters viewParameters;

void main()
{
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * localToWorld * vec4(position, 1.f);
	vsOut.worldSpacePosition = (localToWorld * vec4(position, 1.f)).xyz;
	vsOut.viewSpacePosition = (viewParameters.viewMatrix * vec4(position, 1.f)).xyz;
	vsOut.worldSpaceNormal = normalize((inverse(transpose(localToWorld)) * vec4(normal, 0.f)).xyz);
	vsOut.worldSpaceTangent = normalize((localToWorld * vec4(tangent.xyz, 0.f)).xyz);
	vsOut.tangentSpaceHandedness = tangent.w;
	vsOut.texCoord0 = texCoord0;
	vsOut.texCoord1 = texCoord1;
	vsOut.vertexColor = normal.xyz * .5 + .5;
}
