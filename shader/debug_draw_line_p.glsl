#version 450 core

out vec3 outColor;
uniform vec3 color;

in VSOutput
{
    vec4 color;
	vec3 objectSpacePosition;
} psIn;

void main() {
	outColor = psIn.color.rgb;
}
