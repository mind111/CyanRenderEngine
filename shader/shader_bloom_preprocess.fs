#version 450 core

in vec2 uv;

out vec4 fragcolor;

// input will be super-sample sized 2560 * 1440
uniform sampler2D quadSampler;

float luminance(vec3 color)
{
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

// todo: this function is for attenuating the bloom color to prevent
// those really bright pixels in the original render produce "big bright dots"
// in the blurred bloom image. the basic logic is that the stronger the brightness,
// the stronger the attenuation. there maybe other functions that better fits this
// purpose.
float attenuate(float lumin)
{
    return 1.f / (1.f + 0.3f * lumin);
}

void main()
{
    fragcolor = vec4(0.f, 0.f, 0.f, 1.f);

    // box filter
    vec2 uvOffset = 1.f / textureSize(quadSampler, 0);
    vec2 boxUv_0 = uv + vec2(-uvOffset.x, uvOffset.y);
    vec2 boxUv_1 = uv + vec2(uvOffset.x, uvOffset.y);
    vec2 boxUv_2 = uv + vec2(-uvOffset.x, -uvOffset.y);
    vec2 boxUv_3 = uv + vec2(uvOffset.x, -uvOffset.y);
    vec3 color = texture(quadSampler, boxUv_0).rgb;
    color += texture(quadSampler, boxUv_1).rgb;
    color += texture(quadSampler, boxUv_2).rgb;
    color += texture(quadSampler, boxUv_3).rgb;
    color *= 0.25f;
    float lumin = luminance(color);
    if (lumin > 1.0f)
    {
        // color *= attenuate(lumin);
        fragcolor = vec4(color, 1.f);
    }
}