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

out vec3 outColor;

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
uniform int numFrames;
uniform sampler2D historyBuffer;

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
	vec3 outRadiance = materialSsbo.materials[hit.material].emissive.rgb;
	outRadiance += calcDirectLighting(p, normal, material);
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

vec3 calcIrradiance(vec3 p, vec3 n, in Material material, int numSamples)
{
	float minT = 1.f / 0.f;
	vec3 irradiance = vec3(0.f);
	float ao = 0.f;
	for (int i = 0; i < numSamples; ++i)
	{
		float u = rand(p.xy * float(numSamples));
		float v = rand(p.zy * float(numSamples));
		vec3 sampleDir = uniformSampleHemisphere(n, u, v);
		Ray ray = Ray(p, sampleDir);
		RayHit hit = trace(ray);
		if (hit.tri >= 0)
		{
			vec3 hitNormal = calcNormal(hit);
			if (dot(hitNormal, sampleDir) < 0.f)
			{
				if (hit.t < minT)
					minT = hit.t;
				float ndotl = max(dot(n, ray.rd), 0.f);
				if (hit.t < 0.5f)
					ao += ndotl;
				irradiance += calcDirectLighting(ray.ro + ray.rd * hit.t, hitNormal, material) * ndotl;
			}
		}
	}
	ao /= numSamples;
	return (irradiance / float(numSamples)) * (1.f - ao);
}

vec3 calcDirectLighting(vec3 p, vec3 n, in Material material)
{
	vec3 outRadiance = vec3(0.f);
	// sun light
	vec3 lightColor = vec3(1.f) * 30.f;
	vec3 lightDir = normalize(vec3(1.f));
	float shadow = traceShadow(Ray(p + n * 0.01, lightDir));
	float ndotl = max(dot(lightDir, n), 0.f);
	outRadiance += material.albedo.rgb * ndotl * shadow * lightColor;
	return outRadiance;
}

vec3 calcIndirectLighting(vec3 p, vec3 n, in Material material)
{
	vec3 outRadiance = vec3(0.f);
	outRadiance += calcIrradiance(p + n * 0.01, n, material, 128);
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
		for (int tri = 0; tri < numTriangles; ++tri)
		{
			float t = intersect(
				shadowRay,
				positionSsbo.positions[tri * 3 + 0].xyz,
				positionSsbo.positions[tri * 3 + 1].xyz,
				positionSsbo.positions[tri * 3 + 2].xyz
			);

			if (t > 0.f)
			{
				return 0.f;
			}
		}
	}
	return 1.f;
}

void main()
{
	outColor = vec3(0.f);
	vec2 pixelCoords = gl_FragCoord.xy / outputSize;
	Ray ray = generateRay(pixelCoords);
	RayHit hit = trace(ray);
	if (hit.tri >= 0)
	{
	    vec3 p = camera.position + hit.t * ray.rd;
		vec3 n = calcNormal(hit);
		outColor = shade(ray, hit, materialSsbo.materials[hit.tri]);
	}
	if (numFrames > 0)
	{
		vec3 history = texture(historyBuffer, pixelCoords).rgb;
		outColor = mix(history, outColor, 0.1f);
	}
}
