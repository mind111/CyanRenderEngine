#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;
uniform sampler2D srcTexture;

out vec4 outColor;

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
        float bloomScale = clamp(lumin, 0.0, 1.0);
        // boost the constrast
        outColor = vec4(bloomScale * inColor * 1.0f, 1.f);
    }
}