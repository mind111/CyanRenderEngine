#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outNormal;

uniform float u_improvedNormal;
uniform vec2 u_renderResolution;
uniform mat4 u_viewMatrix;
uniform mat4 u_projectionMatrix;
uniform sampler2D u_sceneDepth;

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) 
{
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
}

vec3 worldToScreen(vec3 pp, in mat4 view, in mat4 projection)
{
    vec4 p = projection * view * vec4(pp, 1.f);
    p /= p.w;
    return p.xyz * .5f + .5f;
}

vec3 transformPositionScreenToWorld(vec3 pp)
{
    vec4 p = inverse(u_projectionMatrix) * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = inverse(u_viewMatrix) * p;
    return p.xyz;
}

// todo: normal refinement
void main()
{
    vec2 pixelCoord = gl_FragCoord.xy;
    vec2 uv = psIn.texCoord0;
    vec2 pixelSize = 1.f / u_renderResolution.xy; 

    float sceneDepth = texture(u_sceneDepth, uv).r;

	if (sceneDepth > 0.999999f) discard;

    vec3 pp = vec3(uv, sceneDepth) * 2.f - 1.f;
	vec3 worldSpacePosition = transformPositionScreenToWorld(pp);

	float facing = 1.f;
	vec3 n, s, t;
	/**
	 * Avoid depth discontinuity using a trick described at https://wickedengine.net/2019/09/22/improved-normal-reconstruction-from-depth/
	 */
    if (u_improvedNormal > 0.5f)
    {
		//     D
		// A - x - C
		//     B
		vec2 coordA = uv + vec2(-pixelSize.x, 0.f);
		float depthA = texture(u_sceneDepth, coordA).r;

		vec2 coordB = uv + vec2(0.f, -pixelSize.y);
		float depthB = texture(u_sceneDepth, coordB).r;

		vec2 coordC = uv + vec2(pixelSize.x, 0.f);
		float depthC = texture(u_sceneDepth, coordC).r;

		vec2 coordD = uv + vec2(0.f, pixelSize.y);
		float depthD = texture(u_sceneDepth, coordD).r;

		if (abs(depthA - sceneDepth) < abs(depthC - sceneDepth))
		{
			vec3 worldSpacePositionA = transformPositionScreenToWorld(vec3(coordA, depthA) * 2.f - 1.f);
			s = normalize(worldSpacePositionA - worldSpacePosition);
			facing *= -1.f;
		}
		else
		{
			vec3 worldSpacePositionC = transformPositionScreenToWorld(vec3(coordC, depthC) * 2.f - 1.f);
			s = normalize(worldSpacePositionC - worldSpacePosition);
		}

		if (abs(depthD - sceneDepth) < abs(depthB - sceneDepth))
		{
			vec3 worldSpacePositionD = transformPositionScreenToWorld(vec3(coordD, depthD) * 2.f - 1.f);
			t = normalize(worldSpacePositionD - worldSpacePosition);
		}
		else
		{
			vec3 worldSpacePositionB = transformPositionScreenToWorld(vec3(coordB, depthB) * 2.f - 1.f);
			t = normalize(worldSpacePositionB - worldSpacePosition);
			facing *= -1.f;
		}
	}
	else 
	{
		s = normalize(dFdx(worldSpacePosition));
		t = normalize(dFdy(worldSpacePosition));
		if (((pixelCoord.x + 1.f) / u_renderResolution.x) > 1.f)
		{
			facing *= -1.f;
		}

		if (((pixelCoord.y + 1.f) / u_renderResolution.y) > 1.f)
		{
			facing *= -1.f;
		}
	}

    n = normalize(cross(s, t)) * facing;
    outNormal = n * .5f + .5f;
}
