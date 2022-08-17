#version 450 core

#define PI 3.1415926
#define FUZZY_OCCLUSION 1
 
in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 ssao; 
out vec3 ssbn;

#define VIEW_SSBO_BINDING 0
layout(std430, binding = VIEW_SSBO_BINDING) buffer ViewShaderStorageBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

uniform sampler2D radianceTexture;
uniform sampler2D depthTexture;
uniform sampler2D normalTexture;
uniform sampler2D blueNoiseTexture;

// halton low discrepancy sequence, from https://www.shadertoy.com/view/wdXSW8
vec2 halton (int index)
{
    const vec2 coprimes = vec2(2.0f, 3.0f);
    vec2 s = vec2(index, index);
	vec4 a = vec4(1,1,0,0);
    while (s.x > 0. && s.y > 0.)
    {
        a.xy = a.xy/coprimes;
        a.zw += a.xy*mod(s, coprimes);
        s = floor(s/coprimes);
    }
    return a.zw;
}

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) {
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
}

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

mat3 tangentToWorld(vec3 n)
{
	vec3 worldUp = abs(n.y) < 0.95f ? vec3(0.f, 1.f, 0.f) : vec3(0.f, 0.f, 1.f);
	vec3 right = cross(n, worldUp);
	vec3 forward = cross(n, right);
	mat3 coordFrame = {
		right,
		forward,
		n
	};
	return coordFrame;
}

vec3 sphericalToCartesian(float theta, float phi, vec3 n)
{
	vec3 localDir = {
		sin(theta) * cos(phi),
		sin(theta) * sin(phi),
		cos(theta)
	};
	return tangentToWorld(n) * localDir;
}

vec3 uniformSampleHemisphere(vec3 n, float u, float v)
{
	float theta = acos(u);
	float phi = 2 * PI * v;
	return normalize(sphericalToCartesian(theta, phi, n));
}

/**
* blue noise samples on a unit disk taken from https://www.shadertoy.com/view/3sfBWs
*/
const vec2 BlueNoiseInDisk[64] = vec2[64](
    vec2(0.478712,0.875764),
    vec2(-0.337956,-0.793959),
    vec2(-0.955259,-0.028164),
    vec2(0.864527,0.325689),
    vec2(0.209342,-0.395657),
    vec2(-0.106779,0.672585),
    vec2(0.156213,0.235113),
    vec2(-0.413644,-0.082856),
    vec2(-0.415667,0.323909),
    vec2(0.141896,-0.939980),
    vec2(0.954932,-0.182516),
    vec2(-0.766184,0.410799),
    vec2(-0.434912,-0.458845),
    vec2(0.415242,-0.078724),
    vec2(0.728335,-0.491777),
    vec2(-0.058086,-0.066401),
    vec2(0.202990,0.686837),
    vec2(-0.808362,-0.556402),
    vec2(0.507386,-0.640839),
    vec2(-0.723494,-0.229240),
    vec2(0.489740,0.317826),
    vec2(-0.622663,0.765301),
    vec2(-0.010640,0.929347),
    vec2(0.663146,0.647618),
    vec2(-0.096674,-0.413835),
    vec2(0.525945,-0.321063),
    vec2(-0.122533,0.366019),
    vec2(0.195235,-0.687983),
    vec2(-0.563203,0.098748),
    vec2(0.418563,0.561335),
    vec2(-0.378595,0.800367),
    vec2(0.826922,0.001024),
    vec2(-0.085372,-0.766651),
    vec2(-0.921920,0.183673),
    vec2(-0.590008,-0.721799),
    vec2(0.167751,-0.164393),
    vec2(0.032961,-0.562530),
    vec2(0.632900,-0.107059),
    vec2(-0.464080,0.569669),
    vec2(-0.173676,-0.958758),
    vec2(-0.242648,-0.234303),
    vec2(-0.275362,0.157163),
    vec2(0.382295,-0.795131),
    vec2(0.562955,0.115562),
    vec2(0.190586,0.470121),
    vec2(0.770764,-0.297576),
    vec2(0.237281,0.931050),
    vec2(-0.666642,-0.455871),
    vec2(-0.905649,-0.298379),
    vec2(0.339520,0.157829),
    vec2(0.701438,-0.704100),
    vec2(-0.062758,0.160346),
    vec2(-0.220674,0.957141),
    vec2(0.642692,0.432706),
    vec2(-0.773390,-0.015272),
    vec2(-0.671467,0.246880),
    vec2(0.158051,0.062859),
    vec2(0.806009,0.527232),
    vec2(-0.057620,-0.247071),
    vec2(0.333436,-0.516710),
    vec2(-0.550658,-0.315773),
    vec2(-0.652078,0.589846),
    vec2(0.008818,0.530556),
    vec2(-0.210004,0.519896) 
);

/**
*/
vec3 blueNoiseSampleHemisphere(vec3 n, vec2 uv, float randomRotation)
{
	// rotate input samples
	mat2 rotation = {
		{ cos(randomRotation), sin(randomRotation) },
		{ -sin(randomRotation), cos(randomRotation) }
	};
	uv = rotation * uv;

	// project points on a unit disk up to the hemisphere
	float z = cos(asin(length(uv)));
	return tangentToWorld(n) * vec3(uv.xy, z);
}

// todo: calculate horizon angle at each azimuthal slice
// todo: calculate bent cone using horizon based approach 
// todo: calculate horizon based ao
// todo: experiement with sdf style "soft" occlusion
// todo: verify calculated bent normal
void main() 
{
    float depth = texture(depthTexture, psIn.texCoord0).r;
    vec3 normal = texture(normalTexture, psIn.texCoord0).xyz;
    normal = normalize(normal * 2.f - 1.f);

    if (depth > .99f) 
		discard;

    vec3 screenSpaceCoord = vec3(psIn.texCoord0 * 2.f - 1.f, depth * 2.f - 1.f);
    vec3 ro = screenToWorld(screenSpaceCoord, inverse(viewSsbo.view), inverse(viewSsbo.projection));
	vec3 viewDirection = normalize(-(viewSsbo.view * vec4(ro, 1.f)).xyz);
	vec3 worldSpaceViewDirection = (inverse(viewSsbo.view) * vec4(viewDirection, 0.f)).xyz;
	ro += normal * 0.005f;

    // ray marching
	float occlusion = 0.f;
	vec3 bentNormal = worldSpaceViewDirection;
	const int numRays = 8;
	// sample in 2m hemisphere
	const float sampleRadius = 2.0f;
	// number of steps to march along the ray
	const int kMaxNumSteps = 8;
    const float stepSize = sampleRadius / float(kMaxNumSteps);
    for (int ray = 0; ray < numRays; ++ray)
    {
		float fuzzyOcclusion = 1.f;
		float occluded = 1.f;
		// vec3 rd = uniformSampleHemisphere(normal, rand(psIn.texCoord0 * vec2(17.9173, 13.3)), rand(psIn.texCoord0.yx * ray));
		float randomRotation = texture(blueNoiseTexture, gl_FragCoord.xy / float(textureSize(blueNoiseTexture, 0).x)).r * PI * 2.f;
		vec3 rd = blueNoiseSampleHemisphere(normal, BlueNoiseInDisk[ray], randomRotation);

		for (int steps = 0; steps < kMaxNumSteps; ++steps)
		{
			vec3 worldSpacePosition = ro + rd * (stepSize * steps);
			// project to screenspace and do depth comparison
			vec4 screenSpacePosition = viewSsbo.projection * viewSsbo.view * vec4(worldSpacePosition, 1.f);
			screenSpacePosition *= (1.f / screenSpacePosition.w);
			screenSpacePosition.z = screenSpacePosition.z * .5f + .5f;

			if (abs(screenSpacePosition.x) <= 1.0f && abs(screenSpacePosition.y) <= 1.0f)
			{
				float stepDepth = texture(depthTexture, screenSpacePosition.xy * .5f + .5f).r;
				if (screenSpacePosition.z >= stepDepth)
				{
					occluded = 0.f;
					break;
				}
			}
			else 
			{
				break;
			}
/*
			// soft occlusion inspired by https://iquilezles.org/articles/rmshadows/
			vec3 stepWorldPosition = screenToWorld(vec3(screenSpacePosition.xy, stepDepth * 2.f - 1.f), inverse(viewSsbo.view), inverse(viewSsbo.projection));
			if (screenSpacePosition.z > stepDepth)
			{
				fuzzyOcclusion = 0.f;
			}
			else 
			{
				fuzzyOcclusion = min(fuzzyOcclusion, 2.f * length(worldSpacePosition - stepWorldPosition) / (stepSize * steps));
			}
*/
		}

		// occlusion += fuzzyOcclusion;
		occlusion += occluded;
		if (occluded > 0.5f)
		{
			bentNormal += rd;
		}
	}
	occlusion = occlusion / float(numRays);

#if 0
	vec3 avgBentNormal = bentNormal / float(numRays);
	/**
	* the shorter the averaged bent normal means the more scattered the unoccluded samples, thus the bigger the variance
	* smaller variance -> visible samples are more focused -> more occlusion 
	*/
	float coneAngle = (1.f - max(0.f, 2 * length(avgBentNormal) - 1.f)) * (PI / 2.f);
	float variance = 1.f - max(0.f, 2 * length(avgBentNormal) - 1.f);
#endif

	bentNormal = normalize(bentNormal);

	ssao = vec3(occlusion);
	ssbn = bentNormal * .5f + .5f;
	// outColor = vec4(vec3(max(dot(bentNormal, normal), 0.f)), 1.f);
	// outColor = vec4(vec3(occlusion), 1.f);
}
