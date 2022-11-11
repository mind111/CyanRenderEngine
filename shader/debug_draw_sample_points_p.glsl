#version 450 core

in VSOut {
	vec3 color; 
} psIn;

out vec3 outColor;
void main() {
	outColor = psIn.color;
}
