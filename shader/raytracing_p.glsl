#version 450 core

#define PI 3.1415926

#define POSITION_BUFFER_BINDING 20
#define NORMAL_BUFFER_BINDING 21
#define MATERIAL_BUFFER_BINDING 22
#define DEBUG_WRITE_BUFFER_BINDING 23

in VSOutput
{
	vec2 texCoord0;
} psIn;

layout (location = 0) out vec3 outColor;
// ReSTIR
layout (location = 1) out vec3 outReservoirPosition;
layout (location = 2) out vec3 outReservoirRadiance;
layout (location = 3) out vec3 outReservoirSamplePosition;
layout (location = 4) out vec3 outReservoirSampleNormal;
layout (location = 5) out vec3 outReservoirWM;
layout (location = 6) out vec3 outDebugOutput;

uniform sampler2D temporalReservoirPosition;
uniform sampler2D temporalReservoirRadiance;
uniform sampler2D temporalReservoirSamplePosition;
uniform sampler2D temporalReservoirSampleNormal;
uniform sampler2D temporalReservoirWM;
uniform sampler2D sceneDepthBuffer;
uniform sampler2D sceneNormalBuffer;

layout(std430, binding = 0) buffer ViewSSBO
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

layout(std430, binding = POSITION_BUFFER_BINDING) buffer PositionSSBO
{
	vec4 positions[];
} positionSsbo;

layout(std430, binding = NORMAL_BUFFER_BINDING) buffer NormalSSBO
{
	vec4 normals[];
} normalSsbo;

struct Material
{	
	vec4 albedo;
	vec4 emissive;
	vec4 roughness;
	vec4 metallic;
};

layout(std430, binding = MATERIAL_BUFFER_BINDING) buffer MaterialSSBO
{
	Material materials[];
} materialSsbo;

uniform vec2 outputSize;
uniform int numTriangles;
uniform uint numFrames;
uniform sampler2D historyBuffer;
uniform sampler2D blueNoiseTexture;

struct Ray
{	
	vec3 ro;
	vec3 rd;
};

struct RayHit
{
	float t;
	int tri;
	int material;
};

struct PerspectiveCamera
{
	vec3 forward;
	vec3 right;
	vec3 up;
	vec3 position;
	vec3 lookAt;
	vec3 worldUp;
	float fov;
	float aspectRatio;
	float n;
	float f;
};

uniform PerspectiveCamera camera;

vec3 calcNormal(in RayHit hit);
float traceShadow(in Ray shadowRay);
vec3 calcIrradiance(vec3 p, vec3 n, int numSamples);
vec3 calcDirectLighting(vec3 p, vec3 n, in Material material);
vec3 calcIndirectLighting(vec3 p, vec3 n, in Material material);

Ray generateRay(vec2 pixelCoords)
{
	vec2 uv = pixelCoords * 2.f - 1.f;
	uv.x *= camera.aspectRatio;
	float h = camera.n * tan(radians(.5f * camera.fov));
	vec3 ro = camera.position;
	vec3 rd = normalize(camera.forward * camera.n + camera.right * uv.x * h + camera.up * uv.y * h);
	return Ray(ro, rd);
}

float intersect(in Ray ray, in vec3 v0, in vec3 v1, in vec3 v2)
{
	const float EPSILON = 0.0000001;
	vec3 edge1 = v1 - v0;
	vec3 edge2 = v2 - v0;
	vec3 h, s, q;
	float a,f,u,v;
	h = cross(ray.rd, edge2);
	a = dot(edge1, h);
	if (abs(a) < EPSILON)
	{
		return -1.0f;
	}
	f = 1.0f / a;
	s = ray.ro - v0;
	u = f * dot(s, h);
	if (u < 0.0 || u > 1.0)
	{
		return -1.0;
	}
	q = cross(s, edge1);
	v = f * (ray.rd.x*q.x + ray.rd.y*q.y + ray.rd.z*q.z);
	if (v < 0.0 || u + v > 1.0)
	{
		return -1.0;
	}
	float t = f * dot(edge2, q);
	// hit
	if (t > EPSILON)
	{
		return t;
	}
	return -1.0f;
}

RayHit trace(in Ray ray)
{
	float closestT = 1.f/0.f;
	int hitTri = -1;
	int material = -1;

	// brute force tracing
	{
		for (int tri = 0; tri < numTriangles; ++tri)
		{
			float t = intersect(
				ray,
				positionSsbo.positions[tri * 3 + 0].xyz,
				positionSsbo.positions[tri * 3 + 1].xyz,
				positionSsbo.positions[tri * 3 + 2].xyz
			);

			if (t > 0.f && t < closestT)
			{
				closestT = t;
				hitTri = tri;
				material = tri;
			}
		}
	}
	return RayHit( closestT, hitTri, material );
}

vec3 shade(in Ray ray, in RayHit hit, in Material material)
{
	vec3 p = ray.ro + ray.rd * hit.t;
	vec3 normal = calcNormal(hit);
	vec3 outRadiance = vec3(0.f);
	outRadiance += materialSsbo.materials[hit.material].emissive.rgb;
	// outRadiance += calcDirectLighting(p, normal, material);
	outRadiance += calcIndirectLighting(p, normal, material);
	return outRadiance;
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

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 uniformSampleHemisphere(vec3 n, float u, float v)
{
	float theta = acos(u);
	float phi = 2 * PI * v;
	vec3 localDir = {
		sin(theta) * cos(phi),
		sin(theta) * sin(phi),
		cos(theta)
	};
	return tangentToWorld(n) * localDir; 
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
* intuitively, I think this maybe really similar to stratified importance sampling the cosine weighted hemisphere if 
* blue noise produce somewhat evenly distributed samples
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

vec3 LambertianDiffuse(vec3 albedo)
{
	return albedo / PI;
}

vec3 calcIrradiance(vec3 p, vec3 n, in Material material, int numSamples)
{
	vec3 irradiance = vec3(0.f);
	float randomRotation = texture(blueNoiseTexture, gl_FragCoord.xy / float(textureSize(blueNoiseTexture, 0).x)).r * PI * 2.f;
	for (int i = 0; i < numSamples; ++i)
	{
		vec3 sampleDir = blueNoiseCosWeightedSampleHemisphere(n, BlueNoiseInDisk[i], randomRotation);

		Ray ray = Ray(p, sampleDir);
		RayHit hit = trace(ray);
		if (hit.tri >= 0)
		{
			vec3 hitNormal = calcNormal(hit);
			if (dot(hitNormal, sampleDir) < 0.f)
			{
				// since we are doing cosine weighted importance sampling, the pi term in the Lambertian BRDF and the cosine term are cancelled
				irradiance += calcDirectLighting(ray.ro + ray.rd * hit.t, hitNormal, materialSsbo.materials[hit.tri]) * material.albedo.rgb;
			}
		}
	}
	irradiance /= float(numSamples);
	return irradiance;
}

float calcLuminance(in vec3 inLinearColor)
{   
    return 0.2126 * inLinearColor.r + 0.7152 * inLinearColor.g + 0.0722 * inLinearColor.b;
}

float random(vec2 uv)
{
	return fract(sin(dot(uv, vec2(12.9898,78.233)))*43758.5453123);
}

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

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) {
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
}

void screenSpaceRayMarching(vec2 texCoord, vec3 ro, vec3 rd) {
	float kMaxMarchDist = 2.f;
	const int kMaxNumSteps = 6;
	float stepSize = kMaxMarchDist / float(kMaxNumSteps);
	for (int i = 0; i < kMaxNumSteps; ++i) {
		
	}
}

struct Reservoir
{
	vec3 position;
	// position of the indirect sample ray hit
	vec3 samplePosition;
	// normal of the indirect sample ray hit
	vec3 sampleNormal;
	vec3 radiance;
	// sum of weights for samples seen by the stream so far
	float w;
	// total number of samples in the stream
	float M;
	float pdf;
};

struct Sample
{
	vec3 position;
	vec3 normal;
	vec3 radiance;
};

void updateReservoir(inout Reservoir reservoir, in Sample inSample, float weight)
{
	reservoir.w += weight;
	reservoir.M += 1.f;
	if (reservoir.w > 0.f) {
		float probability = weight / reservoir.w;
		vec2 rng = get_random();
		if (rng.x < probability) {
			reservoir.samplePosition = inSample.position;
			reservoir.sampleNormal = inSample.normal;
			reservoir.radiance = inSample.radiance;
			reservoir.pdf = calcLuminance(inSample.radiance);
		}
	}
}

void mergeReservoir(vec3 p, inout Reservoir merged, in Reservoir candidate) {
	merged.w += candidate.w;
	if (merged.w > 0.f) {
		float probability = candidate.w / merged.w;
		vec2 rng = get_random();
		if (rng.x < probability) {
			merged.radiance = candidate.radiance;
			merged.M += candidate.M;
			// todo: need to calculate jacobian to account for the fact that the reused neighbor reservoir generates the sample with a different solid angle pdf
			float phi2q = abs(dot(candidate.sampleNormal, normalize(candidate.position - candidate.samplePosition)));
			float phi2r = abs(dot(candidate.sampleNormal, normalize(merged.position - candidate.samplePosition)));
			float jacob = (phi2r / phi2q) * dot(candidate.samplePosition - candidate.position, candidate.samplePosition - candidate.position) / dot(merged.position - candidate.position, merged.position - candidate.position);
			merged.pdf = calcLuminance(merged.radiance) / jacob;
		}
	}
}

vec3 ReSTIREstimator(in Reservoir reservoir) {
	if (reservoir.pdf > 0.f && reservoir.M > 0.f) {
		return reservoir.radiance * (reservoir.w / (reservoir.pdf * reservoir.M));
	}
	return vec3(0.f);
}

/*
* note: the random number generator is quite important for the robustness of the reservoir updating procedure. Theoretically, if M never gets clamped
* then every pixel should eventually converge to their "best" sample, since sum of the weight will grow with more processed samples in the stream
* and the probability for updating a reservoir should keep decreasing. 
* todo: though majority of the pixels converges, not all pixel converges to their "favourite" sample, need to figure out why.
*/
#define CLAMP_M 1
Reservoir ReSTIR(vec3 p, vec3 n, vec2 texCoord, float randomRotation) {
	// initialize the reservoir
	Reservoir reservoir;
	if (numFrames > 0) {
		// todo: reproject current pixel back to last frame to find matching pixel using motion vector
		reservoir.position = texture(temporalReservoirPosition, texCoord).rgb;
		reservoir.radiance = texture(temporalReservoirRadiance, texCoord).rgb;
		reservoir.samplePosition = texture(temporalReservoirSamplePosition, texCoord).rgb;
		reservoir.sampleNormal = texture(temporalReservoirSampleNormal, texCoord).rgb;
		vec2 wM = texture(temporalReservoirWM, texCoord).rg;
		reservoir.w = wM.x;
		reservoir.M = wM.y;
		reservoir.pdf = calcLuminance(reservoir.radiance);
	}
	else {
		reservoir.position = p;
		reservoir.radiance = vec3(0.f);
		reservoir.samplePosition = vec3(0.f);
		reservoir.sampleNormal = vec3(0.f);
		reservoir.w = 0.f;
		reservoir.M = 0.f;
		reservoir.pdf = 0.f;
	}

	// clamp M to stop adding new samples into the stream
	const int kMaxM = 2;
	if (int(reservoir.M) > kMaxM) {
		return reservoir;
	}

	// generate a new sample
	Sample newSample;
	newSample.radiance = vec3(0.f);
	newSample.position = vec3(0.f);
	newSample.normal = vec3(0.f);
	vec2 rng = get_random();
	vec3 rd = uniformSampleHemisphere(n, rng.x, rng.y);
	Ray ray = Ray(p, rd);
	RayHit hit = trace(ray);
	if (hit.tri >= 0) {
		vec3 hitNormal = calcNormal(hit);
		// avoid back face hit
		if (dot(hitNormal, rd) < 0.f) {
			newSample.position = p + rd * hit.t;
			newSample.normal = hitNormal;
			newSample.radiance = calcDirectLighting(ray.ro + ray.rd * hit.t, hitNormal, materialSsbo.materials[hit.tri]);
		}
	}
	float srcPdf = 1.f / (2.f * PI);
	float targetPdf = calcLuminance(newSample.radiance);

	// update the reservoir
	updateReservoir(reservoir, newSample, targetPdf / srcPdf);

	// reuse temporal neighbors
	if (numFrames > 0) {
		float depth = texture(sceneDepthBuffer, texCoord).r;
		vec3 normal = texture(sceneNormalBuffer, texCoord).rgb * 2.f - 1.f;
		// todo: randomize the neighbors and better kernel
		vec2 neighbors[4] = { 
			vec2( 1.f, 0.f),
			vec2( 0.f, 1.f),
			vec2(-1.f, 0.f),
			vec2( 0.f,-1.f),
		};

		// merge neighbor reservoirs
		for (int i = 0; i < 1; ++i) {
			vec2 neighborCoord = texCoord + neighbors[i] * 0.02;

			// address boundry condition
			if (neighborCoord.x < 0.f || neighborCoord.x > 1.f || neighborCoord.y < 0.f || neighborCoord.y > 1.f) {
				continue;
			}

			Reservoir candidate;
			candidate.radiance = texture(temporalReservoirRadiance, neighborCoord).rgb;
			vec2 wM = texture(temporalReservoirWM, neighborCoord).rg; 
			candidate.w = wM.x;
			candidate.M = wM.y;

			float neighborDepth = texture(sceneDepthBuffer, neighborCoord).r;
			vec3 neighborWorldSpacePos = screenToWorld(vec3(texCoord * 2.f - 1.f, neighborDepth * 2.f - 1.f), inverse(viewSsbo.view), inverse(viewSsbo.projection));
			vec3 neighborNormal = texture(sceneNormalBuffer, neighborCoord).rgb * 2.f - 1.f;
			float normalDot = clamp(dot(normal, neighborNormal), 0.f, 1.f);
			if ((acos(normalDot) > (0.14 * PI)) || (abs(depth - neighborDepth) > 0.05f)) {
				continue;
			}
			mergeReservoir(p, reservoir, candidate);
			reservoir.radiance = vec3(1.f, 0.f, 0.f);
		}
		// todo: check visibility
		// todo: do screen space ray march to verify visibility of reused reservoir, if failed, then don't update the reservoir using merged reservoir

		// reuse spatial neighbors iteratively

	}
	outReservoirPosition = reservoir.position;
	outReservoirRadiance = reservoir.radiance;
	outReservoirSamplePosition = reservoir.samplePosition;
	outReservoirSampleNormal = reservoir.sampleNormal;
	outReservoirWM = vec3(reservoir.w, reservoir.M, 0.f);
	return reservoir;
}

#define RAY_BUMP 0.01
vec3 calcDirectLighting(vec3 p, vec3 n, in Material material) {
	vec3 outRadiance = vec3(0.f);
	// sun light
	vec3 lightDir = normalize(vec3(1.f));
	vec3 lightColor = vec3(1.f) * 3.f;
	float shadow = traceShadow(Ray(p + n * RAY_BUMP, lightDir));
	float ndotl = max(dot(lightDir, n), 0.f);
	outRadiance += LambertianDiffuse(material.albedo.rgb) * ndotl * shadow * lightColor;
	return outRadiance;
}

vec3 calcIndirectLighting(vec3 p, vec3 n, in Material material) {
	vec3 outRadiance = vec3(0.f);
	vec2 texCoord = gl_FragCoord.xy;
	float randomRotation = texture(blueNoiseTexture, gl_FragCoord.xy / float(textureSize(blueNoiseTexture, 0).x)).r * PI * 2.f;

	/*
	* ReSTIR GI for estimating diffuse indirect irradiance
	*/
	Reservoir reservoir = ReSTIR(p + n * RAY_BUMP, n, texCoord / outputSize, randomRotation);
	outRadiance += ReSTIREstimator(reservoir);

	// outRadiance += calcIrradiance(p + n * 0.01, n, material, 1);
	return outRadiance;
}

vec3 calcNormal(in RayHit hit)
{
	vec3 n0 = normalSsbo.normals[hit.tri * 3 + 0].xyz;
	vec3 n1 = normalSsbo.normals[hit.tri * 3 + 1].xyz;
	vec3 n2 = normalSsbo.normals[hit.tri * 3 + 2].xyz;
	return normalize(n0 + n1 + n2);
}

float traceShadow(in Ray shadowRay)
{
	// brute force tracing
	{
		for (int tri = 0; tri < numTriangles; ++tri) {
			float t = intersect(
				shadowRay,
				positionSsbo.positions[tri * 3 + 0].xyz,
				positionSsbo.positions[tri * 3 + 1].xyz,
				positionSsbo.positions[tri * 3 + 2].xyz
			);

			if (t > 0.f) {
				return 0.f;
			}
		}
	}
	return 1.f;
}

// todo: visualization to help understand the algorithm better!!!!
void main() {
	vec2 pixelCoord = (gl_FragCoord.xy / outputSize);
	outReservoirPosition = numFrames > 0 ? texture(temporalReservoirPosition, pixelCoord).rgb : vec3(0.f);
	outReservoirRadiance = numFrames > 0 ? texture(temporalReservoirRadiance, pixelCoord).rgb : vec3(0.f);
	outReservoirSamplePosition = numFrames > 0 ? texture(temporalReservoirSamplePosition, pixelCoord).rgb : vec3(0.f);
	outReservoirSampleNormal = numFrames > 0 ? texture(temporalReservoirSampleNormal, pixelCoord).rgb : vec3(0.f);
	outReservoirWM = numFrames > 0 ? texture(temporalReservoirWM, pixelCoord).rgb : vec3(0.f);

	// initialize random number generator state
	seed = numFrames;
	if (numFrames % 30 == 0) {
		flat_idx = uint(floor(gl_FragCoord.y) * 2560 + floor(gl_FragCoord.x));

		outColor = vec3(0.f);
		Ray ray = generateRay(pixelCoord);
		RayHit hit = trace(ray);
		if (hit.tri >= 0) {
			vec3 n = calcNormal(hit);
			if (dot(n, ray.rd) < 0.f) {
				vec3 p = camera.position + hit.t * ray.rd;
				outColor = shade(ray, hit, materialSsbo.materials[hit.tri]);
				outReservoirPosition = numFrames > 0 ? texture(temporalReservoirPosition, pixelCoord).rgb : p;
			}
		}
	}
	outColor = outReservoirRadiance;
}
