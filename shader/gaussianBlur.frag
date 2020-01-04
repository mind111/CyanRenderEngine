#version 450 core
in vec2 textureCoord;
out vec4 blurFragColor;

uniform sampler2D colorBuffer;
// Gaussian weight
uniform float weights[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
uniform bool horizontalPass;
uniform float offset;

void main() {
    blurFragColor = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 fragColor = texture(colorBuffer, textureCoord);
    blurFragColor += fragColor.rgb * weights[0];

    for (int i = 1; i < 5; i++) {
        blurFragColor += texture(colorBuffer, textureCoord + vec2(offset * i, 0.0)).rgb * weights[i];
        blurFragColor += texture(colorBuffer, textureCoord - vec2(offset * i, 0.0)).rgb * weights[i];

        if (!horizontalPass) {
            blurFragColor += texture(colorBuffer, textureCoord + vec2(0.0, offset * i)).rgb * weights[i];
            blurFragColor += texture(colorBuffer, textureCoord - vec2(0.0, offset * i)).rgb * weights[i];
        }
    }
}