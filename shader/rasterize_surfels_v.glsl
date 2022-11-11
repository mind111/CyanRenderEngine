#version 450 core

#define pi 3.14159265359

struct Surfel {
	vec4 positionAndRadius;
	vec4 normal;
	vec4 albedo;
	vec4 radiance;
};

out VSOutput {
	flat Surfel surfel;
	flat vec3 viewSpacePosition;
	flat vec3 viewSpaceNormal;
	flat float pointSize;
} vsOut;

layout(std430, binding = 69) buffer SurfelBuffer {
	Surfel surfels[];
};

uniform mat4 view;
uniform mat4 projection;
uniform float cameraFov;
uniform float cameraN;
uniform ivec2 outputSize;

float calcPointSizeApproximate(vec3 viewSpacePosition, in Surfel surfel, float surfelRadius) {
	float rr = surfelRadius * cameraN / abs(viewSpacePosition.z);
	// convert world space radius to screen space in units of pixels
	float pixelSize = 2.f * tan(radians(cameraFov * .5f)) * cameraN / float(outputSize.y);
	return rr / pixelSize;
}

void main() {
	vec4 position = vec4(surfels[gl_InstanceID].positionAndRadius.xyz, 1.f);
	float radius = surfels[gl_InstanceID].positionAndRadius.w;
	gl_Position = projection * view * position;
	vsOut.surfel = surfels[gl_InstanceID];
	vsOut.viewSpacePosition = (view * position).xyz;
	vsOut.viewSpaceNormal = (inverse(transpose(view))* surfels[gl_InstanceID].normal).xyz;
	vsOut.pointSize = calcPointSizeApproximate(vsOut.viewSpacePosition, vsOut.surfel, radius);
	gl_PointSize = vsOut.pointSize;
}
