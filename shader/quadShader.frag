
#version 450 core
in vec2 textureCoord;
out vec4 fragColor;

uniform sampler2D colorBuffer;

void main() {
    fragColor = texture(colorBuffer, textureCoord); 
}