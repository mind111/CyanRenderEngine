#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec4 outColor;

// camera parameters
uniform vec3 cameraPosition;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform float n;
uniform float f;
uniform float fov;
uniform float aspectRatio;

uniform sampler2D heightFieldTex;

void main()
{
	vec2 pixelCoord = psIn.texCoord0 * 2.f - 1.f;

	vec3 ro = cameraPosition;
	// calculate view ray for current pixel
	vec3 o = cameraPosition + cameraForward * n;
	float d = n * tan(radians(fov / 2.f));
	vec3 u = cameraRight * d * aspectRatio;
	vec3 v = cameraUp * d;
	vec3 p = o + u * pixelCoord.x + v * pixelCoord.y;
	vec3 rd = normalize(p - cameraPosition);

	// transform the view ray from world space to height field's local space

	// solve the intersection of view ray with the AABB of the height field
	vec3 pmin = vec3(-1.f, 0.f, -1.f);
	vec3 pmax = vec3( 1.f, 1.f,  1.f);
	float t0 = -1.f / 0.f, t1 = 1.f / 0.f;

	float txmin = (pmin.x - cameraPosition.x) / rd.x;
	float txmax = (pmax.x - cameraPosition.x) / rd.x;
	float tx0 = min(txmin, txmax);
	float tx1 = max(txmin, txmax);
	t0 = max(tx0, t0);
	t1 = min(tx1, t1);

	float tymin = (pmin.y - cameraPosition.y) / rd.y;
	float tymax = (pmax.y - cameraPosition.y) / rd.y;
	float ty0 = min(tymin, tymax);
	float ty1 = max(tymin, tymax);
	t0 = max(ty0, t0);
	t1 = min(ty1, t1);

	float tzmin = (pmin.z - cameraPosition.z) / rd.z;
	float tzmax = (pmax.z - cameraPosition.z) / rd.z;
	float tz0 = min(tzmin, tzmax);
	float tz1 = max(tzmin, tzmax);
	t0 = max(tz0, t0);
	t1 = min(tz1, t1);

	if (t0 >= 0.f && t0 < t1)
	{
		bool bHit = false;
		vec3 rayEntry = ro + rd * t0;

		float tMax = t1 - t0;
		int numSteps = 32;
		float stepSize = tMax / float(numSteps);
		float t = 0.f;
		// linear ray marching into the height field
		for (int i = 0; i < numSteps; ++i)
		{
			vec3 p = rayEntry + rd * t;
			vec2 uv = (p.xz - vec2(-1.f, 1.f)) / 2.f;
			float height = texture(heightFieldTex, uv).r;
			float rayHeight = p.y;
			if (rayHeight <= height)
			{
				bHit = true;
				break;
			}
			t += stepSize;
		}

		if (bHit)
		{
			// perform shading at found intersection
			vec3 hitPosition = rayEntry + rd * t;
			outColor = vec4(hitPosition, 1.f);
		}
		else
		{
			outColor = vec4(0.f, 0.f, 0.f, 1.f);
		}
	}
	else
	{
		outColor = vec4(0.f, 0.f, 0.f, 1.f);
	}
}