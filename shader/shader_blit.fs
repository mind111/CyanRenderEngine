#version 450 core

in vec2 uv;

out vec4 fragColor;

uniform float exposure;
uniform float bloom;
uniform float bloomIntensity;

uniform sampler2D quadSampler;
uniform sampler2D bloomSampler_0;  // 1280 * 960

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

mat3 ACESInputMat =
{
    {0.59719, 0.35458, 0.04823},
    {0.07600, 0.90834, 0.01566},
    {0.02840, 0.13383, 0.83777}
};

mat3 ACESOutputMat =
{
    { 1.60475, -0.53108, -0.07367},
    {-0.10208,  1.10813, -0.00605},
    {-0.00327, -0.07276,  1.07602}
};

vec3 RRTAndODTFit(vec3 v)
{
    vec3 a = v * (v + 0.0245786f) - 0.000090537f;
    vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

vec3 ACESFitted(vec3 color)
{
    color = transpose(ACESInputMat) * color;
    // Apply RRT and ODT
    color = RRTAndODTFit(color);
    color = transpose(ACESOutputMat) * color;
    // Clamp to [0, 1]
    color = vec3(saturate(color.r), saturate(color.g), saturate(color.b));
    return color;
}

void main()
{
    const float bloomIntensity = .7f;
    vec3 color = texture(quadSampler, uv).rgb;
    if (bloom > 0.5f)
    {
        // TODO: bloomIntensity goes here
        color += 0.65 * texture(bloomSampler_0, uv).rgb * bloomIntensity;
    }
    // tone mapping
    vec3 mappedColor = ACESFilm(exposure * color);
    // gamma correction from linear space to sRGB space
    mappedColor = vec3(pow(mappedColor.r, 1.f/2.2f), pow(mappedColor.g, 1.f/2.2f), pow(mappedColor.b, 1.f/2.2f));
    fragColor = vec4(mappedColor, 1.f);
}