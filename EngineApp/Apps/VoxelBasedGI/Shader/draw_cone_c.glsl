#version 450 core

#define PI 3.1415926
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout (std430, binding = 0) buffer ConeTransformBuffer {
	mat4 models[];
} outConeTransformBuffer;

layout (std430, binding = 1) buffer ConeOffsetBuffer {
	vec4 worldSpacePosition;
	vec4 co;
} outConeOffsetBuffer;

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
uniform sampler2D u_sceneDepthTex;
uniform sampler2D u_sceneNormalTex;
uniform float u_coneNormalOffset;
uniform float u_coneDirectionOffset;

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

vec2 coneAngles[7] = { 
	vec2(0.f, 0.f),
	vec2(30.f, 60.f),
	vec2(30.f, 120.f),
	vec2(30.f, 180.f),
	vec2(30.f, 240.f),
	vec2(30.f, 300.f),
	vec2(30.f, 360.f),
};

vec3 diffuseConeDirections[6] = {	
	vec3(0.f, 1.f, 0.f),
	vec3(0.f, .5f, 0.866025f),
	vec3(0.823639, 0.5, 0.267617),
	vec3(0.509037, 0.5, -0.700629),
	vec3(-0.509037, 0.5f, -0.700629),
	vec3(-0.823639, .5f, 0.267617)
};

void main()
{
	const vec2 coord = vec2(.5f);
	ivec2 icoord = ivec2(coord * textureSize(u_sceneDepthTex, 0).xy);

	float depth = texelFetch(u_sceneDepthTex, icoord, 0).r;
	if (depth > 0.999999f) return;

	vec3 worldSpacePosition = screenToWorld(vec3(coord, depth) * 2.f - 1.f, inverse(viewParameters.viewMatrix), inverse(viewParameters.projectionMatrix));
	vec3 n = normalize(texelFetch(u_sceneNormalTex, icoord, 0).rgb * 2.f - 1.f);
	vec3 co = worldSpacePosition + n * u_coneNormalOffset;

	// write output
	outConeOffsetBuffer.worldSpacePosition = vec4(worldSpacePosition, 1.f);
	outConeOffsetBuffer.co = vec4(co, 1.f);

	#if 0
	// diffuse cone setup #1
	for (int i = 0; i < 7; ++i)
	{
		// what data I need to write?
		vec2 coneAngle = coneAngles[i];
		vec3 coneDirection = sphericalToCartesian(radians(coneAngle.x), radians(coneAngle.y), n);

		mat4 scale;
		float d = .5f;
		float halfAperture = radians(15.f);
		float radius = d * tan(halfAperture);
		scale[0] = vec4(radius, 0.f, 0.f, 0.f);
		scale[1] = vec4(0.f,d / 2.f, 0.f, 0.f);
		scale[2] = vec4(0.f, 0.f, radius, 0.f);
		scale[3] = vec4(0.f, 0.f, 0.f, 1.f);

		// based on coneDirection and cone settings, calculate a transform
		mat4 rotate;
		// derive a rotation matrix form this direction
		vec3 worldUp = vec3(0.f, 1.f, 0.f);
		if (coneDirection.y >= 0.9f)
			worldUp = vec3(0.f, 0.f, 1.f);
		vec3 xAxis = normalize(cross(coneDirection, worldUp));
		vec3 zAxis = normalize(cross(xAxis, coneDirection));
		rotate[0] = vec4(xAxis, 0.f);     // x
		rotate[1] = vec4(coneDirection, 0.f); // y
		rotate[2] = vec4(zAxis, 0.f);     // z
		rotate[3] = vec4(0.f, 0.f, 0.f, 1.f);

		co += coneDirection * u_coneDirectionOffset;

		mat4 translate;
		translate[0] = vec4(1.f, 0.f, 0.f, 0.f);
		translate[1] = vec4(0.f, 1.f, 0.f, 0.f);
		translate[2] = vec4(0.f, 0.f, 1.f, 0.f);
		translate[3] = vec4(co, 1.f);

		// write output
		outConeTransformBuffer.models[i] = translate * rotate * scale;
	}
	#else 
	// diffuse cone setup #2
	for (int i = 0; i < 6; ++i)
	{
		vec3 coneDirection = construct_ONB_frisvad(n) * normalize(vec3(diffuseConeDirections[i].x, diffuseConeDirections[i].z, diffuseConeDirections[i].y));

		mat4 scale;
		float d = .5f;
		float halfAperture = radians(30.f);
		float radius = d * tan(halfAperture);
		scale[0] = vec4(radius, 0.f, 0.f, 0.f);
		scale[1] = vec4(0.f,d / 2.f, 0.f, 0.f);
		scale[2] = vec4(0.f, 0.f, radius, 0.f);
		scale[3] = vec4(0.f, 0.f, 0.f, 1.f);

		// based on coneDirection and cone settings, calculate a transform
		mat4 rotate;
		// derive a rotation matrix form this direction
		vec3 worldUp = vec3(0.f, 1.f, 0.f);
		if (coneDirection.y >= 0.9f)
			worldUp = vec3(0.f, 0.f, 1.f);
		vec3 xAxis = normalize(cross(coneDirection, worldUp));
		vec3 zAxis = normalize(cross(xAxis, coneDirection));
		rotate[0] = vec4(xAxis, 0.f);     // x
		rotate[1] = vec4(coneDirection, 0.f); // y
		rotate[2] = vec4(zAxis, 0.f);     // z
		rotate[3] = vec4(0.f, 0.f, 0.f, 1.f);

		co += coneDirection * u_coneDirectionOffset;

		mat4 translate;
		translate[0] = vec4(1.f, 0.f, 0.f, 0.f);
		translate[1] = vec4(0.f, 1.f, 0.f, 0.f);
		translate[2] = vec4(0.f, 0.f, 1.f, 0.f);
		translate[3] = vec4(co, 1.f);

		// write output
		outConeTransformBuffer.models[i] = translate * rotate * scale;
	}
	#endif
}
