#version 450 core

layout (points) in;
// layout (line_strip, max_vertices = 24) out;
layout (triangle_strip, max_vertices = 36) out;

in gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
} gl_in[];

in VSOutput
{
	int primitiveID;
} gsIn[];

// this is necessary since using seperable program
out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

struct ViewParameters
{
	uvec2 renderResolution;
	float aspectRatio;
	mat4 viewMatrix;
    mat4 prevFrameViewMatrix;
	mat4 projectionMatrix;
    mat4 prevFrameProjectionMatrix;
	vec3 cameraPosition;
	vec3 prevFrameCameraPosition;
	vec3 cameraRight;
	vec3 cameraForward;
	vec3 cameraUp;
	int frameCount;
	float elapsedTime;
	float deltaTime;
};
uniform ViewParameters viewParameters;
uniform sampler3D u_opacity;
uniform sampler3D u_radiance;
uniform vec3 u_voxelGridCenter;
uniform vec3 u_voxelGridExtent;
uniform vec3 u_voxelGridResolution;
uniform vec3 u_voxelGridAABBMin;
uniform vec3 u_voxelGridAABBMax;
uniform vec3 u_voxelSize;
uniform vec2 u_renderResolution;
uniform float u_cameraNearClippingPlane;
uniform float u_cameraFov;

uniform vec3 u_ro;
uniform vec3 u_rd;

void emitWireframeCube(vec3 worldSpaceCubeCenter, float cubeSize)
{
	float cubeExtent = cubeSize * .5f;
	//   h -- g
	//  /|   /|
	// d -- c |
	// | e--|-f
	// |/   |/
	// a -- b
	vec3 va = worldSpaceCubeCenter + vec3(-cubeExtent, -cubeExtent, cubeExtent);
	vec3 vb = worldSpaceCubeCenter + vec3( cubeExtent, -cubeExtent, cubeExtent);
	vec3 vc = worldSpaceCubeCenter + vec3( cubeExtent,  cubeExtent, cubeExtent);
	vec3 vd = worldSpaceCubeCenter + vec3(-cubeExtent,  cubeExtent, cubeExtent);

	vec3 ve = worldSpaceCubeCenter + vec3(-cubeExtent, -cubeExtent, -cubeExtent);
	vec3 vf = worldSpaceCubeCenter + vec3( cubeExtent, -cubeExtent, -cubeExtent);
	vec3 vg = worldSpaceCubeCenter + vec3( cubeExtent,  cubeExtent, -cubeExtent);
	vec3 vh = worldSpaceCubeCenter + vec3(-cubeExtent,  cubeExtent, -cubeExtent);

	// face abcd
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(va, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vb, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vc, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vd, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(va, 1.f);
	EmitVertex();
	EndPrimitive();

	// face efgh
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(ve, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vf, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vg, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vh, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(ve, 1.f);
	EmitVertex();
	EndPrimitive();

	// face bfgc
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vb, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vf, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vg, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vc, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vb, 1.f);
	EmitVertex();
	EndPrimitive();

	// face aehd
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(va, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(ve, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vh, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vd, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(va, 1.f);
	EmitVertex();
	EndPrimitive();
}

void emitCube(vec3 worldSpaceCubeCenter, float cubeSize)
{
	float cubeVertices[] = {
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
	};
	float cubeExtent = cubeSize * .5f;
	for (int f = 0; f < 12; ++f)
	{
		for (int i = 0; i < 3; ++i)
		{
			vec3 v = vec3(cubeVertices[(f * 3 + i) * 3 + 0], cubeVertices[(f * 3 + i) * 3 + 1], cubeVertices[(f * 3 + i) * 3 + 2]) * cubeExtent + worldSpaceCubeCenter;
			gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(v, 1.f);
			EmitVertex();
		}
		EndPrimitive;
	}
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

vec3 calcVoxelUV(vec3 p)
{
    vec3 o = u_voxelGridCenter + vec3(-u_voxelGridExtent.x, -u_voxelGridExtent.y, u_voxelGridExtent.z);
    vec3 uv = (p - o) / (u_voxelGridExtent * 2.f);
    uv.z *= -1.f;
	return uv;
}

bool isValidVoxelUV(vec3 uv)
{
	return (uv.x >= 0.f && uv.x <= 1.f && uv.y >= 0.f && uv.y <= 1.f && uv.z >= 0.f && uv.z <= 1.f);
}

void main()
{
	const float coneAperture = radians(5.f);
	const float distanceToDiameter = tan(coneAperture);

	float tEnter = 0.f, tExit = 0.f;
	if (intersectAABB(u_ro, u_rd, tEnter, tExit, u_voxelGridAABBMin, u_voxelGridAABBMax))
	{
		const int numSteps = 64;
#if 0 // linear stepping
		float dt = (tExit - tEnter) / numSteps;
        vec3 p = ro + rd * tEnter;
		float d = tEnter;
		vec4 acc =  vec4(0.f, 0.f, 0.f, 0.f);
        for (int i = 0; i < numSteps; ++i)
        {
            if (acc.a >= 1.f)
            {
				break;
            }

			p += rd * dt;
			d += dt;

            vec3 voxelUV = calcVoxelUV(p);
            if (isValidVoxelUV(voxelUV))
			{
				float diameter = 2.f * d * distanceToDiameter;
				float mipLevel = max(log2(diameter / u_voxelSize.x), 0.f);
                float opacity = textureLod(u_opacity, voxelUV, mpLevel).r;
                vec3 radiance = textureLod(u_radiance, voxelUV, mipLevel).rgb;

				acc.rgb += (1.f - acc.a) * radiance;
				acc.a += (1.f - acc.a) * opacity;
			}
        }
#else // kind of exponential stepping
#if 1
		float dt = u_voxelSize.x;
        vec3 p = u_ro + u_rd * tEnter;
		float d = tEnter;
		vec4 acc =  vec4(0.f, 0.f, 0.f, 0.f);
        for (int i = 0; i < numSteps; ++i)
        {
            if (acc.a >= 1.f)
            {
				break;
            }

			p += u_rd * dt;
			d += dt;

			float diameter = 2.f * d * distanceToDiameter;
			dt = diameter;
            vec3 voxelUV = calcVoxelUV(p);
            if (isValidVoxelUV(voxelUV))
			{
				float mipLevel = max(log2(diameter / u_voxelSize.x), 0.f);
                float opacity = textureLod(u_opacity, voxelUV, mipLevel).r;
                vec3 radiance = textureLod(u_radiance, voxelUV, mipLevel).rgb;

				acc.rgb += (1.f - acc.a) * radiance;
				acc.a += (1.f - acc.a) * opacity;

				if (gsIn[0].primitiveID == i)
				{
					// emit a cube at sampling position 
					emitWireframeCube(p, diameter);
				}
			}
        }
#else
	emitCube(u_ro + u_rd * gsIn[0].primitiveID, u_voxelSize.x);
#endif
#endif
	}
}
