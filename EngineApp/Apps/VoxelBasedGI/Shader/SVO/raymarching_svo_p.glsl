#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outColor;

uniform vec3 u_SVOCenter;  
uniform vec3 u_SVOExtent;  
uniform vec3 u_SVOAABBMin;
uniform vec3 u_SVOAABBMax;
uniform int u_SVOMaxLevel;
uniform vec2 u_renderResolution;
uniform float u_cameraNearClippingPlane;
uniform float u_cameraFov;
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

uniform sampler3D u_voxelAlbedoPool;
uniform sampler3D u_voxelNormalPool;
uniform sampler3D u_voxelDirectLightingPool;

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
HitRecord rayMarchingSVO(vec3 ro, vec3 rd, vec3 SVOCenter, vec3 SVOExtent, vec3 SVOAABBMin, vec3 SVOAABBMax)
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
		SVONode current = descendSVO(calcSVOUV(p, SVOCenter, SVOExtent), u_SVOMaxLevel - 1, nodeIndex);

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
			current = descendSVO(calcSVOUV(p, SVOCenter, SVOExtent), u_SVOMaxLevel - 1, nodeIndex);
		}
	}

	return outHitRecord;
}

void main()
{
	// calculate rd
	vec2 uv = gl_FragCoord.xy / u_renderResolution;
	uv = uv * 2.f - 1.f;
	vec3 cy = u_cameraNearClippingPlane * tan(radians(u_cameraFov * .5f)) * viewParameters.cameraUp;
	float aspect = u_renderResolution.x / u_renderResolution.y;
	vec3 cx = u_cameraNearClippingPlane * tan(radians(u_cameraFov * .5f)) * aspect * viewParameters.cameraRight;
	vec3 rd = normalize(uv.x * cx  + uv.y * cy + u_cameraNearClippingPlane * viewParameters.cameraForward);

	vec3 ro = viewParameters.cameraPosition;
	HitRecord hitRecord = rayMarchingSVO(ro, rd, u_SVOCenter, u_SVOExtent, u_SVOAABBMin, u_SVOAABBMax);
	if (hitRecord.bHit)
	{
		vec3 uvColor = hitRecord.node.coord.xyz / pow(2.f, hitRecord.node.coord.w);
		ivec3 voxelCoord = calcVoxelCoord(int(hitRecord.node.childIndex), textureSize(u_voxelAlbedoPool, 0));
		vec3 albedo = texelFetch(u_voxelAlbedoPool, voxelCoord, 0).rgb;
		vec3 normal = texelFetch(u_voxelNormalPool, voxelCoord, 0).rgb;
		vec3 directLighting = texelFetch(u_voxelDirectLightingPool, voxelCoord, 0).rgb;
		// outColor = uvColor;
		// outColor = albedo;
		outColor = directLighting;
	}
}
