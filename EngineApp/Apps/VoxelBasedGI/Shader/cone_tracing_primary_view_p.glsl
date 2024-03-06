#version 450 core

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
uniform int u_maxMarchingSteps;

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
	// calculate rd
	vec2 uv = gl_FragCoord.xy / u_renderResolution;
	uv = uv * 2.f - 1.f;
	vec3 cy = u_cameraNearClippingPlane * tan(radians(u_cameraFov * .5f)) * viewParameters.cameraUp;
	float aspect = u_renderResolution.x / u_renderResolution.y;
	vec3 cx = u_cameraNearClippingPlane * tan(radians(u_cameraFov * .5f)) * aspect * viewParameters.cameraRight;
	vec3 rd = normalize(uv.x * cx  + uv.y * cy + u_cameraNearClippingPlane * viewParameters.cameraForward);
	vec3 ro = viewParameters.cameraPosition;

	outColor = vec3(0.f);

	const float coneAperture = radians(.1f);
	const float distanceToDiameter = 2.f * tan(coneAperture);

	float tEnter = 0.f, tExit = 0.f;
	if (intersectAABB(ro, rd, tEnter, tExit, u_voxelGridAABBMin, u_voxelGridAABBMax))
	{
		const int numSteps = u_maxMarchingSteps;
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
				float diameter = d * distanceToDiameter;
				float mipLevel = max(log2(diameter / u_voxelSize.x), 0.f);
                float opacity = textureLod(u_opacity, voxelUV, mipLevel).r;
                vec3 radiance = textureLod(u_radiance, voxelUV, mipLevel).rgb;

				acc.rgb += (1.f - acc.a) * radiance;
				acc.a += (1.f - acc.a) * opacity;
			}
        }
#else // kind of exponential stepping
		float dt = u_voxelSize.x;
        vec3 p = ro + rd * tEnter;
		float d = tEnter;
		vec4 acc =  vec4(0.f, 0.f, 0.f, 0.f);
        // for (int i = 0; i < numSteps; ++i)
		while (d < 100.f)
        {
            if (acc.a >= 1.f)
            {
				break;
            }

			p += rd * dt;
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
			}
        }
#endif

		outColor = acc.rgb;
    }
}
