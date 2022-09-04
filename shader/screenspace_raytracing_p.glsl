#version 450 core

#define PI 3.1415926
#define FUZZY_OCCLUSION 1
 
in VSOutput
{
	vec2 texCoord0;
} psIn;

layout (location = 0) out vec3 ssao; 
layout (location = 1) out vec3 ssbn;
layout (location = 2) out vec3 ssr;
layout (location = 3) out vec3 ssaoCount;

#define VIEW_SSBO_BINDING 0
layout(std430, binding = VIEW_SSBO_BINDING) buffer ViewShaderStorageBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

uniform uint numFrames;
uniform vec2 outputSize;
uniform sampler2D radianceTexture;
uniform sampler2D depthTexture;
uniform sampler2D normalTexture;
uniform sampler2D blueNoiseTexture;
uniform samplerCube reflectionProbe;
uniform sampler2D temporalSsaoBuffer;
uniform sampler2D temporalSsaoCountBuffer;
uniform float kRoughness;

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
	float z = cos(asin(length(uv)));
	return tangentToWorld(n) * vec3(uv.xy, z);
}

void rayMarching(vec3 ro, vec3 rd) { 

}

#define AO_USE_INTERMEDIATE_SAMPLES 0
// todo: improve this by applying spatiotemporal reuse
void calculateOcclusionAndBentNormal(vec3 p, vec3 n, vec3 worldSpaceViewDirection) {
    // ray marching
	float occlusion = 0.f;
    float occ = 0.f;
	vec3 bentNormal = worldSpaceViewDirection;
	const int numRays = 4;
#if AO_USE_INTERMEDIATE_SAMPLES
    int numSamples = 0;
#endif
	// sample in 1m hemisphere (randomize to turn banding into noise)
	float sampleRadius = 1.f;
    sampleRadius *= get_random().x;
	// number of steps to march along the ray
    // todo: instead of using fixed step size, stratify and jitter step size to eliminate aliasing
	const int kMaxNumSteps = 8;
    const float stepSize = sampleRadius / float(kMaxNumSteps);
    for (int ray = 0; ray < numRays; ++ray)
    {
		float occluded = 1.f;
		float randomRotation = texture(blueNoiseTexture, gl_FragCoord.xy / float(textureSize(blueNoiseTexture, 0).x)).r * PI * 2.f;
		vec3 rd = blueNoiseCosWeightedSampleHemisphere(n, BlueNoiseInDisk[ray], randomRotation);

		for (int steps = 0; steps < kMaxNumSteps; ++steps)
		{
			vec3 worldSpacePosition = p + rd * (stepSize * steps);
			// project to screenspace and do depth comparison
			vec4 screenSpacePosition = viewSsbo.projection * viewSsbo.view * vec4(worldSpacePosition, 1.f);
			screenSpacePosition *= (1.f / screenSpacePosition.w);
			screenSpacePosition.z = screenSpacePosition.z * .5f + .5f;

            // outside of view frustum
			if (abs(screenSpacePosition.x) <= 1.0f && abs(screenSpacePosition.y) <= 1.0f) {
				float sceneDepth = texture(depthTexture, screenSpacePosition.xy * .5f + .5f).r;
				if (screenSpacePosition.z >= sceneDepth)
				{
					occluded = 0.f;
                    occ += 1.f;
					break;
				}
#if AO_USE_INTERMEDIATE_SAMPLES
                else {
                    // if there is some geometry discovered by the marched ray, treat that as occlusion as long as it's within the uppder hemisphere of current shaded pixel
                    vec3 sampleWorldSpacePosition = screenToWorld(vec3(screenSpacePosition.xy, sceneDepth * 2.f - 1.f), inverse(viewSsbo.view), inverse(viewSsbo.projection));
                    if (sceneDepth < 0.99f) {
                        if (dot(normalize(sampleWorldSpacePosition - p), n) > 0.1f) {
                            occ += dot(normalize(sampleWorldSpacePosition - p), n);
                            numSamples++;
                        }
                    }
                }
#endif 
			}
			else {
				break;
			}
		}

		occlusion += occluded;
		if (occluded > 0.5f)
		{
			bentNormal += rd;
		}
#if AO_USE_INTERMEDIATE_SAMPLES
        numSamples++;
#endif
	}
	occlusion = occlusion / float(numRays);
#if AO_USE_INTERMEDIATE_SAMPLES
    occ = occ / float(numSamples);
#endif
	bentNormal = normalize(bentNormal);
#if 0
	vec3 avgBentNormal = bentNormal / float(numRays);
	/**
	* the shorter the averaged bent normal means the more scattered the unoccluded samples, thus the bigger the variance
	* smaller variance -> visible samples are more focused -> more occlusion 
	*/
	float coneAngle = (1.f - max(0.f, 2 * length(avgBentNormal) - 1.f)) * (PI / 2.f);
	float variance = 1.f - max(0.f, 2 * length(avgBentNormal) - 1.f);
#endif

#if AO_USE_INTERMEDIATE_SAMPLES
    ssao = vec3(1.f - occ);
#else
	ssao = vec3(occlusion);
#endif
	ssbn = bentNormal * .5f + .5f;
};

void temporalSsao(vec3 p, vec3 n) {
    float occlusion = 0.f;
	const int numNewSamples = 1;
    vec2 pixelCoord = gl_FragCoord.xy / vec2(2560.f, 1440.f);
    float numAccSamples = numFrames > 0 ? texture(temporalSsaoCountBuffer, pixelCoord).r : 0.f;
    if (numAccSamples > 8.f) {
        ssao = vec3(1.f - texture(temporalSsaoBuffer, pixelCoord).r);
    }

	// sample in 1m hemisphere (randomize to turn banding into noise)
	float sampleRadius = 1.f;
    sampleRadius *= get_random().x;

	// number of steps to march along the ray
    // todo: instead of using fixed step size, stratify and jitter step size to eliminate aliasing
	const int kMaxNumSteps = 8;
    const float stepSize = sampleRadius / float(kMaxNumSteps);

    for (int ray = 0; ray < numNewSamples; ++ray) {
		float randomRotation = texture(blueNoiseTexture, gl_FragCoord.xy / float(textureSize(blueNoiseTexture, 0).x)).r * PI * 2.f;
		vec3 rd = blueNoiseCosWeightedSampleHemisphere(n, BlueNoiseInDisk[ray], randomRotation);

		for (int steps = 0; steps < kMaxNumSteps; ++steps) {
			vec3 worldSpacePosition = p + rd * (stepSize * steps);
			// project to screenspace and do depth comparison
			vec4 screenSpacePosition = viewSsbo.projection * viewSsbo.view * vec4(worldSpacePosition, 1.f);
			screenSpacePosition *= (1.f / screenSpacePosition.w);
			screenSpacePosition.z = screenSpacePosition.z * .5f + .5f;

            // outside of view frustum
			if (abs(screenSpacePosition.x) <= 1.0f && abs(screenSpacePosition.y) <= 1.0f) {
				float sceneDepth = texture(depthTexture, screenSpacePosition.xy * .5f + .5f).r;
				if (screenSpacePosition.z >= sceneDepth) {
                    occlusion += 1.f;
					break;
				}
			}
			else {
				break;
			}
		}
	}

#if 1
    if (numFrames > 0) { 
		// reuse temporal neighbors
		const int kNumReuseNeighbors = 8;
		const float filterRadius = .1f;
        vec2 neighbors[4] = { 
            vec2(1.f, 0.f),
            vec2(-1.f, 0.f),
            vec2(0.f, 1.f),
            vec2(0.f, -1.f)
        };
		for (int i = 0; i < 4; ++i) {
			/*
			float randomRotation = texture(blueNoiseTexture, gl_FragCoord.xy / float(textureSize(blueNoiseTexture, 0).x)).r * PI * 2.f;
			mat2 rotation = {
				{ cos(randomRotation), sin(randomRotation) },
				{ -sin(randomRotation), cos(randomRotation) }
			};
            */
			vec2 neighborCoord = pixelCoord + neighbors[i] * filterRadius;
			if (neighborCoord.x < 0.f || neighborCoord.x > 1.f || neighborCoord.y < 0.f || neighborCoord.y > 1.f) {
				continue;
			}
			float occ = texture(temporalSsaoBuffer, neighborCoord).r;
			float numSamples = texture(temporalSsaoCountBuffer, neighborCoord).r;
			occlusion += occ * numSamples;
			// numAccSamples += numSamples;
		}
		occlusion /= numAccSamples;
		ssaoCount = vec3(float(numAccSamples));
	} else {
		ssaoCount = vec3(float(numNewSamples));
    }
#endif
	ssao = vec3(1 - occlusion);
}

vec3 importanceSampleGGX(vec3 n, float roughness, float rand_u, float rand_v) {
	float a = roughness * roughness;
	float phi = 2 * PI * rand_u;
	float cosTheta = sqrt( (1 - rand_v) / ( 1 + (a*a - 1) * rand_v ) );
	float sinTheta = sqrt( 1 - cosTheta * cosTheta );
	vec3 h;
	h.x = sinTheta * sin( phi );
	h.y = sinTheta * cos( phi );
	h.z = cosTheta;
	vec3 up = abs(n.y) < 0.999 ? vec3(0, 1.f, 0.f) : vec3(0.f, 0, 1.f);
	vec3 tangentX = normalize( cross( n, up ) );
	vec3 tangentY = cross( n, tangentX);
	// tangent to world space
	return tangentX * h.x + tangentY * h.y + n * h.z;
}

vec3 calculateReflection(vec3 p, vec3 n, vec3 v) {
    const int kNumSamples = 8;
    const int kMaxNumSteps = 8;
    const float kMaxTraceDist = 5.0f;
    float dt = kMaxTraceDist / float(kMaxNumSteps);
    vec3 radiance = vec3(0.f);
    for (int i = 0; i < kNumSamples; ++i) {
        vec2 randUV = get_random();
        vec3 h = importanceSampleGGX(n, kRoughness, randUV.x, randUV.y);
        vec3 l = normalize(-reflect(v, h));
		vec3 li = texture(reflectionProbe, l).rgb;
        // ray marching
        for (int steps = 0; steps < kMaxNumSteps; ++steps) {
            vec3 q = p + float(steps) * dt * l;
            vec4 screenSpacePosition = viewSsbo.projection * viewSsbo.view * vec4(q, 1.f);
            screenSpacePosition /= screenSpacePosition.w;
            screenSpacePosition.z = screenSpacePosition.z * .5f + .5f;

            if (abs(screenSpacePosition.x) <= 1.f && abs(screenSpacePosition.y) <= 1.f) {
				float sceneDepth = texture(depthTexture, screenSpacePosition.xy * .5f + .5f).r;
                // found a hit
                if (screenSpacePosition.z >= sceneDepth) {
                    li = vec3(.95, .95, .8);
                    break;
                }
            }
        }
		radiance += li * max(dot(n, l), 0.f); 
    }
    radiance /= float(kNumSamples);
    return radiance;
}

// todo: verify calculated bent normal
void main() {
    seed = 0;
    flat_idx = int(floor(gl_FragCoord.y) * 2560.f + floor(gl_FragCoord.x));

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
	
	// ssao and bent normal
	calculateOcclusionAndBentNormal(ro, normal, worldSpaceViewDirection);
    // temporalSsao(ro, normal);

    // ssr
    // ssr = calculateReflection(ro, normal, worldSpaceViewDirection);
}
