#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outAO;

uniform sampler2D u_sceneDepth;
uniform sampler2D u_sceneNormal;
uniform sampler2D u_reservoirDirection;
uniform sampler2D u_reservoirWSumMWT;
uniform vec2 u_renderResolution;

uniform mat4 u_viewMatrix;
uniform mat4 u_projectionMatrix;

vec3 transformPositionScreenToWorld(vec3 pp)
{
    vec4 p = inverse(u_projectionMatrix) * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = inverse(u_viewMatrix) * p;
    return p.xyz;
}

struct Reservoir {
	vec3 direction;
	float wSum;
	float M;
	float W;
	float targetPdf;
};

void main() 
{
	vec2 pixelCoord = gl_FragCoord.xy;
	vec2 uv = pixelCoord / u_renderResolution;

	float depth = texture(u_sceneDepth, uv).r;
	if (depth >= 0.999999f) discard;

	vec3 worldSpacePosition = transformPositionScreenToWorld(vec3(uv, depth) * 2.f - 1.f);
	vec3 n = texture(u_sceneNormal, uv).rgb * 2.f - 1.f;

	Reservoir r;
	r.direction = texture(u_reservoirDirection, uv).rgb;

	vec3 wSumMW = texture(u_reservoirWSumMWT, uv).rgb;
	r.wSum = wSumMW.x;
	r.M = wSumMW.y;
	r.W = wSumMW.z;

	// float visibility = max(dot(n, r.direction), 0.f) * r.W;
	float visibility = r.W;

	outAO = vec3(visibility);
}
