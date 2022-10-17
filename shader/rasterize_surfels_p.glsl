#version 450 core

struct Surfel {
	vec4 position;
	vec4 normal;
	vec4 color;
};

in VSOutput {
	Surfel surfel;
	vec3 viewPosition;
	float pointSize;
} psIn;

out vec3 outColor;

uniform vec3 receivingNormal;

void main() {
	Surfel surfel = psIn.surfel;
	vec3 view = normalize(psIn.viewPosition - surfel.position.xyz);
	float geometric = max(dot(surfel.normal.xyz, view), 0.f) * max(dot(-view, receivingNormal), 0.f);
	outColor = geometric * surfel.color.rgb;
	outColor = surfel.color.rgb;
	// outColor = vec3(psIn.pointSize);
}
