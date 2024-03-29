#version 450 core

in vec3 worldSpacePosition;

uniform vec3 lightPosition;
uniform float farClippingPlane;

void main() {
	float linearDepth = clamp(length(worldSpacePosition - lightPosition) / farClippingPlane, 0.f, 1.f);
	gl_FragDepth = linearDepth;
}