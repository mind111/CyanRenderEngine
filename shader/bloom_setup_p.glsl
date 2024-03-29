#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;
out vec4 outColor;

uniform sampler2D srcTexture;

float luminance(vec3 color)
{
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main()
{
    outColor = vec4(0.f, 0.f, 0.f, 1.f);
    vec3 inColor = texture(srcTexture, psIn.texCoord0).rgb;
    float lumin = luminance(inColor);
    // non-thresholded bloom
    if (lumin > 0.0f)
    {
        // boost the constrast using luminance
        float bloomScale = smoothstep(0.0, 1.0, lumin);
        outColor = vec4(bloomScale * bloomScale * inColor * 0.04f, 1.f);
    }
}