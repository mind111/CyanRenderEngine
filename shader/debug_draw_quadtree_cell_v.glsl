#version 450 core

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out VSOutput
{
    vec4 color;
} vsOut;

uniform vec2 cellCenter;
uniform float cellSize;
uniform vec3 cellColor;
uniform mat4 cameraView;
uniform mat4 cameraProjection;
uniform int mipLevel;
uniform sampler2D depthQuadtree;

struct ViewParameters
{
	uvec2 renderResolution;
	float aspectRatio;
	mat4 viewMatrix;
	mat4 projectionMatrix;
	vec3 cameraPosition;
	vec3 cameraRight;
	vec3 cameraForward;
	vec3 cameraUp;
	int frameCount;
	float elapsedTime;
	float deltaTime;
};

uniform ViewParameters viewParameters;

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
	float cellDepth = textureLod(depthQuadtree, cellCenter, mipLevel).r;
	// vec3 worldSpacePosition = screenToWorld(vec3(cellCenter, cellDepth) * 2.f - 1.f, inverse(cameraView), inverse(cameraProjection));
	gl_Position = vec4(vec3(cellCenter, cellDepth) * 2.f - 1.f, 1.f);
	gl_PointSize = pow(2.f, mipLevel) * viewParameters.aspectRatio;
	// gl_PointSize = 10.f;
	vsOut.color = vec4(cellColor, 1.f);
}
