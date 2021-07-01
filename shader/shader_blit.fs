#version 450 core

in vec2 uv;

out vec4 fragColor;

uniform float exposure;
uniform sampler2D quadSampler;

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

// reference: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    vec3 result = (x*(a*x+b))/(x*(c*x+d)+e);
    return vec3(saturate(result.r), saturate(result.g), saturate(result.b)); 
}

void main()
{
    vec3 color = texture(quadSampler, uv).rgb;
    // tone mapping
    vec3 mappedColor = ACESFilm(exposure * color);
    // gamma correction from linear space to sRGB space
    mappedColor = vec3(pow(mappedColor.r, 1.f/2.2f), pow(mappedColor.g, 1.f/2.2f), pow(mappedColor.b, 1.f/2.2f));
    fragColor = vec4(mappedColor, 1.f);
}