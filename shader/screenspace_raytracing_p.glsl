#version 450 core

#define PI 3.1415926
 
in VSOutput
{
	vec2 texCoord0;
} psIn;

layout (location = 0) out vec3 ssao; 
layout (location = 1) out vec3 ssbn;

layout(std430) buffer ViewBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
};

uniform vec2 outputSize;
uniform sampler2D depthTexture;
uniform sampler2D normalTexture;
uniform sampler2D blueNoiseTexture;

/* note: 
* rand number generator taken from https://www.shadertoy.com/view/4lfcDr
*/
uint flat_idx;
uint seed;
void encrypt_tea(inout uvec2 arg)
{
	uvec4 key = uvec4(0xa341316c, 0xc8013ea4, 0xad90777d, 0x7e95761e);
	uint v0 = arg[0], v1 = arg[1];
	uint sum = 0u;
	uint delta = 0x9e3779b9u;

	for(int i = 0; i < 32; i++) {
		sum += delta;
		v0 += ((v1 << 4) + key[0]) ^ (v1 + sum) ^ ((v1 >> 5) + key[1]);
		v1 += ((v0 << 4) + key[2]) ^ (v0 + sum) ^ ((v0 >> 5) + key[3]);
	}
	arg[0] = v0;
	arg[1] = v1;
}

vec2 get_random()
{
  	uvec2 arg = uvec2(flat_idx, seed++);
  	encrypt_tea(arg);
  	return fract(vec2(arg) / vec2(0xffffffffu));
}

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

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) 
{
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
}

vec3 worldToScreen(vec3 pp, mat4 view, mat4 projection)
{
    vec4 p = projection * view * vec4(pp, 1.f);
    p /= p.w;
    p.z = p.z * .5f + .5f;
    return p.xyz;
}

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

mat3 tangentToWorld(vec3 n)
{
	vec3 worldUp = abs(n.y) < 0.99f ? vec3(0.f, 1.f, 0.f) : vec3(0.f, 0.f, -1.f);
	vec3 right = cross(worldUp, n);
	vec3 forward = cross(right, n);
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
* using blue noise to do importance sampling hemisphere
*/
vec3 blueNoiseCosWeightedSampleHemisphere(vec3 n, vec2 uv, float randomRotation)
{
	// rotate input samples
	mat2 rotation = {
		{ cos(randomRotation), sin(randomRotation) },
		{ -sin(randomRotation), cos(randomRotation) }
	};
	uv = rotation * uv;

	// project points on a unit disk up to the hemisphere
	float z = sin(acos(length(uv)));
	return tangentToWorld(n) * normalize(vec3(uv.xy, z));
}

// todo: improve this by applying spatiotemporal reuse
void calcAmbientOcclusionAndBentNormal(vec3 p, vec3 n, inout float ao, inout vec3 bentNormal) 
{
	float occlusion = 0.f;
    int numOccludedSamples = 0;
    int numOccludedRays = 0;
	const int kNumRays = 8;
	const int kMaxNumSteps = 8;
	const float kRadius = 1.0f; // 1 meters
    float maxStepSize = kRadius / kMaxNumSteps;
    for (int ray = 0; ray < kNumRays; ++ray)
    {
		float occluded = 1.f;
		float randomRotation = texture(blueNoiseTexture, gl_FragCoord.xy / float(textureSize(blueNoiseTexture, 0).xy)).r * PI * 2.f;
		vec3 rd = blueNoiseCosWeightedSampleHemisphere(n, BlueNoiseInDisk[ray], randomRotation);

		for (int steps = 0; steps < kMaxNumSteps; ++steps)
		{
            float t = float(steps) * maxStepSize + get_random().x * maxStepSize;
			vec3 worldSpacePosition = p + rd * t;
			// project to screenspace and do depth comparison
			vec4 screenSpacePosition = projection * view * vec4(worldSpacePosition, 1.f);
			screenSpacePosition *= (1.f / screenSpacePosition.w);
			screenSpacePosition.z = screenSpacePosition.z * .5f + .5f;

            // inside of view frustum
			if (abs(screenSpacePosition.x) <= 1.0f && abs(screenSpacePosition.y) <= 1.0f) 
            {
				float sceneDepth = texture(depthTexture, screenSpacePosition.xy * .5f + .5f).r;
				if (screenSpacePosition.z >= sceneDepth)
				{
                    // this sample ray direction is either occluded or hidden but as an approximation, 
                    // we treated as occluded
					occluded = 0.f;
                    numOccludedSamples += 1;
				}
			}
			else 
            {
				break;
			}
		}
		if (occluded > 0.5f)
		{
			bentNormal += rd;
		}
        else
        {
            numOccludedRays += 1;
        }
	}
    // ao = float(numOccludedSamples) / float(kMaxNumSteps * kNumRays);
    ao = float(numOccludedRays) / float(kNumRays);
	bentNormal = normalize(bentNormal);
};

void main() 
{
    seed = 0;
    flat_idx = int(floor(gl_FragCoord.y) * outputSize.x + floor(gl_FragCoord.x));

    float depth = texture(depthTexture, psIn.texCoord0).r;
    vec3 normal = texture(normalTexture, psIn.texCoord0).xyz;
    normal = normalize(normal * 2.f - 1.f);

    if (depth > .9999f) 
		discard;

    vec3 screenSpaceCoord = vec3(psIn.texCoord0 * 2.f - 1.f, depth * 2.f - 1.f);
    vec3 ro = screenToWorld(screenSpaceCoord, inverse(view), inverse(projection));
	ro += normal * 0.01f;
	vec3 viewDirection = normalize(-(view * vec4(ro, 1.f)).xyz);
	vec3 worldSpaceViewDirection = (inverse(view) * vec4(viewDirection, 0.f)).xyz;
	
	// ssao and bent normal
    float ao = 1.f;
    vec3 bentNormal = worldSpaceViewDirection;
	calcAmbientOcclusionAndBentNormal(ro, normal, ao, bentNormal);
    ao = 1.f - ao;
    ssao = vec3(ao);
    ssbn = bentNormal * .5f + .5f;
}
