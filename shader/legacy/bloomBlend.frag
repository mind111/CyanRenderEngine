#version 450 core

in vec2 textureCoords;
out vec4 hdrColor;

uniform sampler2D colorBuffer;
uniform sampler2D brightColorBuffer;

vec3 saturate(vec3 v) {
    return clamp(v, vec3(0.0), vec3(1.0)); 
}

// Taken from https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return saturate((x*(a*x+b))/(x*(c*x+d)+e));
}

void main() {
    vec3 result = texture(colorBuffer, textureCoords).rgb;
    result += texture(brightColorBuffer, textureCoords).rgb;

    // tone-mapping
    result = ACESFilm(result);

    // gamma-correction
    result = pow(result, vec3(1.0 / 2.2));
    hdrColor = vec4(result, 1.0);
}