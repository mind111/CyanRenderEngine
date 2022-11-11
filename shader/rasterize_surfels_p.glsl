#version 450 core

struct Surfel {
	vec4 positionAndRadius;
	vec4 normal;
	vec4 albedo;
	vec4 radiance;
};

in VSOutput {
	Surfel surfel;
	flat vec3 viewSpacePosition;
	flat vec3 viewSpaceNormal;
	float pointSize;
} psIn;

out vec3 outColor;

uniform mat4 view;
uniform vec3 hemicubeNormal;

void main() {
	Surfel surfel = psIn.surfel;
	vec3 viewDir = normalize(psIn.viewSpacePosition);
	/** note - @mind:
	* since the direction -view is in view space, need to make sure that surfel.normal is also transformed into view space
	* or else the geometric term calculation will be wrong!
	*/
	vec3 viewSpaceHemicubeNormal = normalize((inverse(transpose(view)) * vec4(hemicubeNormal, 0.f)).xyz);
	float geometric = max(dot(viewDir, viewSpaceHemicubeNormal), 0.f) * max(dot(-viewDir, psIn.viewSpaceNormal), 0.f);
	outColor = surfel.radiance.rgb * geometric;
}
