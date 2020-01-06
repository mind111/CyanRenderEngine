#version 450 core
in vec2 textureCoord;
layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec4 brightColor;

uniform sampler2D colorBuffer;

vec3 grayscaleCoefs = vec3(0.2126, 0.7152, 0.0722);

void main() {
    fragColor = texture(colorBuffer, textureCoord);
    if (dot(fragColor.xyz, grayscaleCoefs) > 0.9) {
        brightColor = fragColor;
    } else {
        brightColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}