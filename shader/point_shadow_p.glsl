#version 450 core

in vec3 worldSpacePosition;

out vec3 linearDepth;

uniform vec3 lightPosition;
uniform float farClippingPlane;

void main() {
	linearDepth = vec3(length(worldSpacePosition - lightPosition) / farClippingPlane);
	gl_FragDepth = linearDepth.r;
}