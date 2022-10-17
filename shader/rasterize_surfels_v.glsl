#version 450 core

#define pi 3.14159265359

struct Surfel {
	vec4 position;
	vec4 normal;
	vec4 color;
};

out VSOutput {
	flat Surfel surfel;
	flat vec3 viewPosition;
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
uniform float surfelRadius;

float calcPointSizeApproximate(vec3 viewPosition, in Surfel surfel) {
	float cosine = max(dot(normalize(viewPosition - surfel.position.xyz), surfel.normal.xyz), 0.f); 
	// approximate the area of  that
	float approxArea = (surfelRadius * surfelRadius * pi) * cosine;
	float r = sqrt(approxArea / pi);
	// screen space radius of projected disk
	vec3 viewSpacePos = (view * surfel.position).xyz;
	float rr = r * cameraN / abs(viewSpacePos.z);
	// convert world space radius to screen space in units of pixels
	float pixelSize = 2.f * tan(radians(cameraFov * .5f)) * cameraN / float(outputSize.y);
	return rr / pixelSize;
}

// todo: find out how to calculate the exact projected area of a disk onto the image plane using perspective projection
float calcPointSizeExact(vec3 viewPosition, in Surfel surfel) {
	return 0.f;
}

void main() {
	gl_Position = projection * view * surfels[gl_InstanceID].position;
	vsOut.surfel = surfels[gl_InstanceID];
	vsOut.viewPosition = view[3].xyz;
	vsOut.pointSize = calcPointSizeApproximate(vsOut.viewPosition, vsOut.surfel);
	// gl_PointSize = 10.f;
	gl_PointSize = vsOut.pointSize;
}
