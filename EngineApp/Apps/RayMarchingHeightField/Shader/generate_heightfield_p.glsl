#version 450 core

#define PI 3.1415926

layout (location = 0) out float outBaseLayerHeight;
layout (location = 1) out vec3 outBaseLayerNormal;
layout (location = 2) out float outSnowLayerHeight;
layout (location = 3) out vec3 outSnowLayerNormal;
layout (location = 4) out float outCompositedHeight;
layout (location = 5) out vec3 outCompositedNormal;
layout (location = 6) out float outCloudOpacity;

in VSOutput
{
	vec2 texCoord0;
} psIn;

vec3 mod289( vec3 x)
{
	return x - floor (x * (1.0 / 289.0)) * 289.0;
}
vec2 mod289( vec2 x)
{
	return x - floor (x * (1.0 / 289.0)) * 289.0;
}
vec3 permute( vec3 x)
{
	return mod289(((x*34.0)+1.0) *x);
}

float simplexNoise2D( vec2 v)
{
	const vec4 C = vec4 (0.211324865405187, // (3.0- sqrt(3.0))/6.0
		0.366025403784439, // 0.5*( sqrt(3.0) -1.0)
		-0.577350269189626 , // -1.0 + 2.0 * C.x
		0.024390243902439); // 1.0 / 41.0
	// First corner
	vec2 i = floor (v + dot(v, C.yy));
	vec2 x0 = v - i + dot(i, C.xx);
	// Other corners
	vec2 i1 = (x0.x > x0.y) ? vec2 (1.0, 0.0) : vec2 (0.0, 1.0);
	vec4 x12 = x0.xyxy + C.xxzz;
	x12.xy -= i1;
	// Permutations
	i = mod289 (i); // Avoid truncation effects in permutation
	vec3 p = permute( permute( i.y + vec3 (0.0, i1.y, 1.0 ))
	+ i.x + vec3 (0.0, i1.x, 1.0 ));
	vec3 m = max (0.5 - vec3( dot(x0,x0), dot(x12.xy ,x12.xy),
	dot(x12.zw,x12.zw)), 0.0);
	m = m * m;
	m = m * m;
	// Gradients
	vec3 x = 2.0 * fract (p * C.www) - 1.0;
	vec3 h = abs(x) - 0.5;
	vec3 a0 = x - floor (x + 0.5);
	// Normalize gradients implicitly by scaling m
	m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );
	// Compute final noise value at P
	vec3 g;
	g.x = a0.x * x0.x + h.x * x0.y;
	g.yz = a0.yz * x12.xz + h.yz * x12.yw;
	return 130.0 * dot(m, g);
}

uniform float u_amplitude;
uniform float u_frequency;
uniform float u_persistence;
uniform int u_numOctaves;
uniform float u_octaveRotationAngles[16];

uniform float u_snowBlendMin;
uniform float u_snowBlendMax;
uniform float snowLayerAmplitude;
uniform float snowLayerFrequency;
uniform float snowLayerPersistence;
uniform int snowLayerNumOctaves;

uniform float u_cloudAmplitude;
uniform float u_cloudFrequency;
uniform int u_cloudNumOctaves;
uniform float u_cloudPersistence;

// todo: randomly rotating each octave
float noise2D(vec2 p, float amplitude, float frequency, int numOctaves, float persistence)
{
	float total = 0.f;
	float maxValue = 0.f;
	for (int i = 0; i < numOctaves; ++i) 
	{
		float angle = u_octaveRotationAngles[i];
		mat2 octaveRotationMat = {
			{ cos(angle), sin(angle) },
			{ -sin(angle), cos(angle) }
		};
		p = octaveRotationMat * p;
		total += simplexNoise2D(p * frequency) * amplitude;
		maxValue += amplitude;
		amplitude *= persistence;
		frequency *= 2;
	}
	return (total / maxValue) * .5f + .5f;
}

uniform float snowHeightBias;

float calcBaseLayerHeight(vec2 coord)
{
	return noise2D(coord, u_amplitude, u_frequency, u_numOctaves, u_persistence);
}

float calcSnowLayerHeight(vec2 coord)
{
	return noise2D(coord, snowLayerAmplitude, snowLayerFrequency, snowLayerNumOctaves, snowLayerPersistence) + snowHeightBias;
}

float calcCompositedHeight(vec2 coord)
{
	float baseLayerHeight = calcBaseLayerHeight(coord);
	float snowLayerHeight = calcSnowLayerHeight(coord);
	float snowHeightBlendFactor = smoothstep(u_snowBlendMin, u_snowBlendMax, baseLayerHeight);
	float compositedHeight = snowLayerHeight > baseLayerHeight ? mix(baseLayerHeight, snowLayerHeight, snowHeightBlendFactor) : baseLayerHeight;
	return compositedHeight;
}

vec3 calcBaseLayerNormal(vec2 coord)
{
	vec2 px = coord + vec2(0.001f, 0.f);
	vec2 py = coord + vec2(0.f, 0.001f);
	vec3 p = vec3(coord, calcBaseLayerHeight(coord));

	vec3 ppx = normalize(vec3(px, calcBaseLayerHeight(px)) - p);
	vec3 ppy = normalize(vec3(py, calcBaseLayerHeight(py)) - p);
	vec3 n = normalize(cross(ppx, ppy));
	return vec3(n.x, n.z, -n.y);
}

vec3 calcCompositedNormal(vec2 coord)
{
	vec2 px = coord + vec2(0.001f, 0.f);
	vec2 py = coord + vec2(0.f, 0.001f);
	vec3 p = vec3(coord, calcCompositedHeight(coord));

	vec3 ppx = normalize(vec3(px, calcCompositedHeight(px)) - p);
	vec3 ppy = normalize(vec3(py, calcCompositedHeight(py)) - p);
	vec3 n = normalize(cross(ppx, ppy));
	return vec3(n.x, n.z, -n.y);
}

void main()
{
	vec2 coord = psIn.texCoord0 * 2.f - 1.f;

	outBaseLayerHeight = calcBaseLayerHeight(coord);
	outBaseLayerNormal = calcBaseLayerNormal(coord);

	outCompositedHeight = calcCompositedHeight(coord);
	outCompositedNormal = calcCompositedNormal(coord);

	outCloudOpacity = noise2D(coord, u_cloudAmplitude, u_cloudFrequency, u_cloudNumOctaves, u_cloudPersistence);
}
