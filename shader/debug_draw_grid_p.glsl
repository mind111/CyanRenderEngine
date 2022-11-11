#version 450 core

out vec3 outColor;
in VSOut {
	vec3 color;
} psIn;

void main() {
	outColor = psIn.color;
}
