#version 450 core
in vec2 textureCoord;
layout (location = 0) out vec4 fragColor;

uniform sampler2D colorBuffer;

vec3 grayscaleCoefs = vec3(0.2126, 0.7152, 0.0722);

void main() {
    fragColor = texture(colorBuffer, textureCoord);
}