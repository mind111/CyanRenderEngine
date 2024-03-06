#version 450 core

#define PI 3.1415926

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outColor;

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
uniform sampler2D u_sceneDepth;
uniform sampler2D u_sceneNormal;
uniform sampler2D u_sceneAlbedo;
uniform sampler2D u_sceneMetallicRoughness;

uniform vec3 u_SVOCenter;  
uniform vec3 u_SVOExtent;  
uniform vec3 u_SVOAABBMin;
uniform vec3 u_SVOAABBMax;
uniform int u_SVONumLevels; 
uniform sampler3D u_voxelAlbedoPool;
uniform sampler3D u_voxelNormalPool;
uniform sampler3D u_voxelDirectLightingPool;
uniform int u_sampleCount;
uniform sampler2D u_prevFrameSceneColor;
uniform samplerCube u_skyLight;
uniform vec3 u_voxelSize;

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

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) 
{
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
}

/* note: 
* tangent to world space transform taken from https://www.shadertoy.com/view/4lfcDr
*/
mat3 construct_ONB_frisvad(vec3 normal)
{
	mat3 ret;
	ret[2] = normal;
    // if normal.z == -1.f
	if(normal.z < -0.999805696) {
		ret[0] = vec3(0.0, -1.0, 0.0);
		ret[1] = vec3(-1.0, 0.0, 0.0);
	}
	else {
		float a = 1.0 / (1.0 + normal.z);
		float b = -normal.x * normal.y * a;
		ret[0] = vec3(1.0 - normal.x * normal.x * a, b, -normal.x);
		ret[1] = vec3(b, 1.0 - normal.y * normal.y * a, -normal.y);
	}
	return ret;
}

vec3 sphericalToCartesian(float theta, float phi, vec3 n)
{
	vec3 localDir = {
		sin(theta) * cos(phi),
		sin(theta) * sin(phi),
		cos(theta)
	};
	return construct_ONB_frisvad(n) * localDir;
}

vec3 uniformSampleHemisphere(vec3 n)
{
    vec2 uv = get_random();
	float theta = acos(uv.x);
	float phi = 2 * PI * uv.y;
	return normalize(sphericalToCartesian(theta, phi, n));
}

struct SVONode
{
	ivec4 coord;
	uint bSubdivide;
	uint bLeaf;
	uint childIndex;
	uint padding;
};

layout (std430) buffer SVONodeBuffer {
	SVONode nodes[];
};

// uv should be in normalized 3d uv space
SVONode descendSVO(vec3 uv, int level, inout uint nodeIndex)
{
	SVONode node = nodes[0];
	nodeIndex = 0;

	for (int i = 0; i < level; ++i)
	{
		if (node.bSubdivide < 1)
		{
			break;
		}

		int childLevel = node.coord.w + 1;
		ivec3 childLevelSize = ivec3(pow(2, childLevel));

		ivec3 firstChildCoord = 2 * node.coord.xyz;
		ivec3 childNodeCoord = ivec3(floor(uv * childLevelSize)) - firstChildCoord;
		int childOffset = childNodeCoord.z * 4 + childNodeCoord.y * 2 + childNodeCoord.x;
		uint childIndex = node.childIndex + childOffset; 
		node = nodes[childIndex];
		nodeIndex = childIndex;
	}

	return node;
}

bool intersectAABB(vec3 ro, vec3 rd, inout float t0, inout float t1, vec3 pmin, vec3 pmax)
{
	bool bHit = false;

	t0 = -1.f / 0.f, t1 = 1.f / 0.f;

	float txmin = (rd.x == 0.f) ? -1e10 : (pmin.x - ro.x) / rd.x;
	float txmax = (rd.x == 0.f) ?  1e10 : (pmax.x - ro.x) / rd.x;
	float tx0 = min(txmin, txmax);
	float tx1 = max(txmin, txmax);
	t0 = max(tx0, t0);
	t1 = min(tx1, t1);

	float tymin = (rd.y == 0.f) ? -1e10 : (pmin.y - ro.y) / rd.y;
	float tymax = (rd.y == 0.f) ?  1e10 : (pmax.y - ro.y) / rd.y;
	float ty0 = min(tymin, tymax);
	float ty1 = max(tymin, tymax);
	t0 = max(ty0, t0);
	t1 = min(ty1, t1);

	float tzmin = (rd.z == 0.f) ? -1e10 : (pmin.z - ro.z) / rd.z;
	float tzmax = (rd.z == 0.f) ?  1e10 : (pmax.z - ro.z) / rd.z;
	float tz0 = min(tzmin, tzmax);
	float tz1 = max(tzmin, tzmax);
	t0 = max(tz0, t0);
	t1 = min(tz1, t1);
	t0 = max(t0, 0.f);

	if (t0 < t1)
	{
		bHit = true;
	}
	return bHit;
}

struct HitRecord
{
	bool bHit;
	SVONode node;
	uint nodeIndex;
	vec3 worldSpacePosition;
};

vec3 calcSVOOrigin(vec3 SVOCenter, vec3 SVOExtent)
{
	return SVOCenter + vec3(-SVOExtent.xy, SVOExtent.z);
}

vec3 calcSVOUV(vec3 p, vec3 SVOCenter, vec3 SVOExtent)
{
	vec3 SVOOrigin = SVOCenter + vec3(-SVOExtent.xy, SVOExtent.z);
	vec3 uv = (p - SVOOrigin) / (2.f * SVOExtent);
	uv.z *= -1.f;
	return uv;
}

ivec3 calcVoxelCoord(int index, ivec3 voxelPoolSize)
{
	int z = index / (voxelPoolSize.x * voxelPoolSize.y);
	int y = (index % (voxelPoolSize.x * voxelPoolSize.y)) / voxelPoolSize.x;
	int x = (index % (voxelPoolSize.x * voxelPoolSize.y)) % voxelPoolSize.x;
	return ivec3(x, y, z);
}

// todo: optimization
// todo: how to deal with offset the ray leading to leaking
HitRecord trace(vec3 ro, vec3 rd, vec3 SVOCenter, vec3 SVOExtent, vec3 SVOAABBMin, vec3 SVOAABBMax)
{
	HitRecord outHitRecord;
	outHitRecord.bHit = false;
	outHitRecord.node = nodes[0];
	outHitRecord.nodeIndex = 0;
	outHitRecord.worldSpacePosition = vec3(0.f);

	vec3 SVOOrigin = calcSVOOrigin(SVOCenter, SVOExtent);

	float tEnter, tExit;
	if (intersectAABB(ro, rd, tEnter, tExit, SVOAABBMin, SVOAABBMax))
	{
		float t = 0.001f;
		vec3 p = ro + rd * (tEnter + t);

		uint nodeIndex = 0;
		SVONode current = descendSVO(calcSVOUV(p, SVOCenter, SVOExtent), u_SVONumLevels - 1, nodeIndex);

		for (int i = 0; i < 128; ++i)
		{
			if (t >= (tExit - tEnter))
			{
				break;
			}

			// if current node is not a leaf node
			bool bIsVoxel = (current.bLeaf > 0 && current.bSubdivide > 0);
			if (!bIsVoxel)
			{
				// march through current node; 
				vec3 dt0, dt1, dt;
				vec3 nodeSize = (SVOExtent * 2.f) / pow(2.f, current.coord.w);
				vec3 q0 = SVOOrigin + current.coord.xyz * vec3(nodeSize.xy, -nodeSize.z);
				vec3 q1 = SVOOrigin + (current.coord.xyz + 1) * vec3(nodeSize.xy, -nodeSize.z);
				dt0 = (q0 - p) / rd;
				dt1 = (q1 - p) / rd;
				dt = max(dt0, dt1);
				float tmin = min(min(dt.x, dt.y), dt.z);

				// offset p along the ray slightly(1mm) to avoid having it stuck at node boundary
				tmin += 0.001f;

				// marching
				p += tmin * rd;
				t += tmin;
			}
			else
			{
				// stop marching if we hit a leaf node
				outHitRecord.bHit = true;
				outHitRecord.node = current;
				outHitRecord.nodeIndex = nodeIndex;
				outHitRecord.worldSpacePosition = p;
				break;
			}

			// todo: repetitively descending the svo from root is slow, we can leverage some
			// spatial coherence in the node that we access here
			// update node
			current = descendSVO(calcSVOUV(p, SVOCenter, SVOExtent), u_SVONumLevels - 1, nodeIndex);
		}
	}

	return outHitRecord;
}

vec3 fetchSVODirectLighting(in SVONode node)
{
	vec3 directLighting = vec3(0.f);
	if (node.bLeaf > 0 && node.bSubdivide > 0)
	{
		ivec3 voxelCoord = calcVoxelCoord(int(node.childIndex), textureSize(u_voxelDirectLightingPool, 0));
		directLighting = texelFetch(u_voxelDirectLightingPool, voxelCoord, 0).rgb;
	}
	return directLighting;
}

vec3 fetchSVONormal(in SVONode node)
{
	vec3 normal = vec3(0.f);
	if (node.bLeaf > 0 && node.bSubdivide > 0)
	{
		ivec3 voxelCoord = calcVoxelCoord(int(node.childIndex), textureSize(u_voxelNormalPool, 0));
		normal = texelFetch(u_voxelNormalPool, voxelCoord, 0).rgb;
	}
	return normal;
}

vec3 fetchSVODiffuseAlbedo(in SVONode node)
{
	vec3 diffuseAlbedo = vec3(0.f);
	if (node.bLeaf > 0 && node.bSubdivide > 0)
	{
		ivec3 voxelCoord = calcVoxelCoord(int(node.childIndex), textureSize(u_voxelNormalPool, 0));
		diffuseAlbedo = texelFetch(u_voxelAlbedoPool, voxelCoord, 0).rgb;
	}
	return diffuseAlbedo;
}

vec3 calcIndirectDiffuse(vec3 p, vec3 n, vec3 diffuseAlbedo)
{
	vec3 outIndirectDiffuse = vec3(0.f);
	const float normalOffset = u_voxelSize.x;
	const float rayOffset = u_voxelSize.x;
	vec3 ro = p + normalOffset * n;
	vec3 rd = uniformSampleHemisphere(n);
	HitRecord hitRecord = trace(ro + rd * rayOffset, rd, u_SVOCenter, u_SVOExtent, u_SVOAABBMin, u_SVOAABBMax);

	if (hitRecord.bHit)
	{
		vec3 directLighting = fetchSVODirectLighting(hitRecord.node);
		outIndirectDiffuse += directLighting * max(dot(n, rd), 0.f);

		// second bounce, sb in short for second bounce
		vec3 sbRo = hitRecord.worldSpacePosition;
		vec3 hitAlbedo = fetchSVODiffuseAlbedo(hitRecord.node);
		vec3 hitNormal = fetchSVONormal(hitRecord.node);
		vec3 sbRd = uniformSampleHemisphere(hitNormal);
		HitRecord sbHitRecord = trace(sbRo + hitNormal * normalOffset, sbRd, u_SVOCenter, u_SVOExtent, u_SVOAABBMin, u_SVOAABBMax);

		if (sbHitRecord.bHit)
		{
			vec3 sbDirectLighting = fetchSVODirectLighting(sbHitRecord.node);
			// todo: second bounce need to be modulated by surface color at first bounce hit
			outIndirectDiffuse += sbDirectLighting * max(dot(hitNormal, sbRd), 0.f) * hitAlbedo;
		}
		else
		{
			vec3 sbSkyRadiance = texture(u_skyLight, sbRd).rgb;
			outIndirectDiffuse += sbSkyRadiance * max(dot(hitNormal, sbRd), 0.f) * hitAlbedo;
		}
	}
	else
	{
		vec3 skyRadiance = texture(u_skyLight, rd).rgb;
		outIndirectDiffuse += skyRadiance * max(dot(n, rd), 0.f);
	}
	return outIndirectDiffuse * diffuseAlbedo;
}

vec3 importanceSampleGGX(vec3 n, float roughness, vec2 xi)
{
	float a = roughness;

	float phi = 2 * PI * xi.x;
	float cosTheta = sqrt((1 - xi.y) / ( 1 + (a*a - 1) * xi.y));
	float sinTheta = sqrt(1 - cosTheta * cosTheta);

	vec3 h;
	h.x = sinTheta * cos( phi );
	h.y = sinTheta * sin( phi );
	h.z = cosTheta;

	vec3 up = abs(n.z) < 0.999 ? vec3(0, 0.f, 1.f) : vec3(1.f, 0, 0.f);
	vec3 tangentX = normalize( cross( up, n ) );
	vec3 tangentY = cross( n, tangentX);
	// tangent to world space
	return tangentX * h.x + tangentY * h.y + n * h.z;
}

float GGX(float roughness, float ndoth) 
{
    float alpha = roughness;
    float alpha2 = alpha * alpha;
    float result = max(alpha2, 1e-6); // prevent the nominator goes to 0 when roughness equals 0
    float denom = ndoth * ndoth * (alpha2 - 1.f) + 1.f;
    result /= (PI * denom * denom); 
    return result;
}

vec3 fresnel(vec3 f0, float vdoth)
{
    float fresnelCoef = 1.f - vdoth;
    fresnelCoef = fresnelCoef * fresnelCoef * fresnelCoef * fresnelCoef * fresnelCoef;
    // f0: fresnel reflectance at incidence angle 0
    // f90: fresnel reflectance at incidence angle 90, f90 in this case is vec3(1.f) 
    vec3 f90 = vec3(1.f);
    return mix(f0, f90, fresnelCoef);
}

float fresnelDielectric(float vdoth)
{
	float f0 = 0.04;
    float fresnelCoef = 1.f - vdoth;
    fresnelCoef = fresnelCoef * fresnelCoef * fresnelCoef * fresnelCoef * fresnelCoef;
	float fr = mix(f0, 1.f, fresnelCoef);
	return fr;
}

float V_SmithGGXHeightCorrelated(float ndotv, float ndotl, float roughness)
{
    float a2 = roughness * roughness;
    float v = ndotl * sqrt(ndotv * ndotv * (1.0 - a2) + a2);
    float l = ndotv * sqrt(ndotl * ndotl * (1.0 - a2) + a2);
    return 0.5f / max((v + l), 1e-5);
}

vec3 calcReflection(vec3 p, vec3 n, vec3 specularAlbedo, float metallic, float roughness)
{
	vec3 outReflection = vec3(0.f);
	const float normalOffset = u_voxelSize.x;
	const float rayOffset = u_voxelSize.x;
	vec3 ro = p + normalOffset * n;
	vec3 h = importanceSampleGGX(n, roughness, get_random());
	vec3 viewDirection = normalize(viewParameters.cameraPosition - p);
	vec3 rd = -reflect(viewDirection, h);
	HitRecord hitRecord = trace(ro + rd * rayOffset, rd, u_SVOCenter, u_SVOExtent, u_SVOAABBMin, u_SVOAABBMax);

	vec3 reflectedRadiance = vec3(0.f);
	float ndotl = max(dot(n, rd), 0.f);
	if (hitRecord.bHit)
	{
		vec3 directLighting = fetchSVODirectLighting(hitRecord.node);
		reflectedRadiance = directLighting;
	}
	else
	{
		vec3 skyRadiance = texture(u_skyLight, rd).rgb;
		reflectedRadiance = skyRadiance;
	}
	float ndotv = clamp(dot(n, viewDirection), 0.f, 1.f);
	float vdoth = clamp(dot(viewDirection, h), 0.f, 1.f);
	float ndoth = clamp(dot(n, h), 0.f, 1.f);
	float V = V_SmithGGXHeightCorrelated(ndotv, ndotl, roughness);
	vec3 F = fresnel(specularAlbedo, vdoth);
	// the max(, 0.1f) is for suppressing the specular fireflies
	outReflection = reflectedRadiance * V * F * vdoth / max(ndoth * ndotv, 0.1);
	return outReflection;
}

void main()
{
    seed = viewParameters.frameCount;
    flat_idx = int(floor(gl_FragCoord.y) * viewParameters.renderResolution.x + floor(gl_FragCoord.x));

	float depth = texture(u_sceneDepth, psIn.texCoord0).r;
	if (depth > .999999f) 
	{
		discard;
	}

	vec3 n = normalize(texture(u_sceneNormal, psIn.texCoord0).rgb * 2.f - 1.f);
	mat4 view = viewParameters.viewMatrix, projection = viewParameters.projectionMatrix;
	vec3 worldSpacePosition = screenToWorld(vec3(psIn.texCoord0, depth) * 2.f - 1.f, inverse(view), inverse(projection));
	vec3 viewDirection = -normalize(worldSpacePosition - viewParameters.cameraPosition);

	const float normalOffset = u_voxelSize.x;
	const float rayOffset = u_voxelSize.x;

	vec3 albedo = texture(u_sceneAlbedo, psIn.texCoord0).rgb;
	vec3 MRO = texture(u_sceneMetallicRoughness, psIn.texCoord0).rgb;
	vec3 diffuseAlbedo = (1.f - MRO.r) * albedo;
	vec3 specularAlbedo = mix(vec3(0.0), albedo, MRO.r);

	vec3 color = vec3(0.f);
	// indirect diffuse
	vec3 h = normalize(n + viewDirection);
	float vdoth = abs(dot(viewDirection, h));
	color += calcIndirectDiffuse(worldSpacePosition, n, diffuseAlbedo) * (1.f - fresnelDielectric(vdoth));
	// indirect specular
	color += calcReflection(worldSpacePosition, n, specularAlbedo, MRO.r, MRO.g * MRO.g);

	// accumulate samples, this only happens in the view stays static
	vec3 accumulatedSampleColor = vec3(0.f);
	if (viewParameters.frameCount > 0)
	{
		accumulatedSampleColor = texture(u_prevFrameSceneColor, psIn.texCoord0).rgb;
	}
	if (u_sampleCount < 1024)
	{
		accumulatedSampleColor = (accumulatedSampleColor * u_sampleCount + color) / (u_sampleCount + 1);
	}
	outColor = accumulatedSampleColor;
}