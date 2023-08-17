#version 450 core

#define PI 3.1415926

out vec3 outBaseLayerAlbedo;

in VSOutput
{
	vec2 texCoord0;
} psIn;

uniform sampler2D u_baseLayerHeightMap;
uniform sampler2D u_baseLayerNormlMap;

uniform float u_snowBlendMin;
uniform float u_snowBlendMax;

uniform vec3 u_groundAlbedo;
uniform vec3 u_snowAlbedo;
uniform vec3 u_grassAlbedo;

vec3 calcBaseLayerAlbedo(vec3 position, vec3 normal)
{
	vec3 outAlbedo;
	float snowHeightBlendFactor = smoothstep(u_snowBlendMin, u_snowBlendMax, position.y);
	float snowNormalBlendFactor = normal.y;
	float snowBlendFactor = snowHeightBlendFactor * snowNormalBlendFactor;
	outAlbedo = mix(u_groundAlbedo, u_snowAlbedo, snowBlendFactor);
	return outAlbedo;
}

void main()
{
	vec2 uv = psIn.texCoord0;
	float baseLayerHeight = texture(u_baseLayerHeightMap, uv).r;
	vec3 position = vec3(uv * 2.f - 1.f, baseLayerHeight);
	vec3 normal = texture(u_baseLayerNormlMap, uv).rgb * 2.f - 1.f;
	outBaseLayerAlbedo = calcBaseLayerAlbedo(position, normal);
}
