#version 450 core

in vec3 worldSpacePosition;

out vec2 outDepthMoments;

uniform vec3 lightPosition;
uniform float farClippingPlane;

void main() {
	float linearDepth = clamp(length(worldSpacePosition - lightPosition) / farClippingPlane, 0.f, 1.f);
	float linearDepthSquared = linearDepth * linearDepth;
	outDepthMoments = vec2(linearDepth, linearDepthSquared);
}
