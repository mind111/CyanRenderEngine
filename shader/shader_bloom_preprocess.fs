#version 450 core

in vec2 uv;

out vec4 fragColor;

// input will be super-sample sized 2560 * 1440
uniform sampler2D quadSampler;

float luminance(vec3 color)
{
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

// TODO: This function is for attenuating the bloom color to prevent
// those really bright pixels in the original render produce "big bright dots"
// in the blurred bloom image. The basic logic is that the stronger the brightness,
// the stronger the attenuation. There maybe other functions that better fits this
// purpose.
float attenuate(float lumin)
{
    return 1.f / (1.f + 0.3f * lumin);
}

void main()
{
    vec3 color = texture(quadSampler, uv).rgb;
    fragColor = vec4(0.f, 0.f, 0.f, 1.f);
    float lumin = luminance(color);
    if (lumin > 0.0f)
    {
        color *= attenuate(lumin);
        fragColor = vec4(color, 1.f);
    }
}