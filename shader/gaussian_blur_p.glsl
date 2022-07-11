#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;
uniform sampler2D srcTexture;

#if 0
layout(location = 0) out vec4 fragColor; 
layout(location = 1) out float singleChannelColor; 

uniform float horizontal;
uniform int kernelIndex;
uniform int radius;

// different gaussian kernel for different downsample/upscale pass
// TODO: kernel size needs more tweaking!!
float kernels[6][9] = {
    { 0.38, 0.24, 0.06, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },            // 0
    { 0.21, 0.18, 0.13, 0.07, 0.0, 0.0, 0.0, 0.0, 0.0},            // 1
    { 0.14, 0.13, 0.11, 0.08, 0.05, 0.03, 0.0, 0.0, 0.0},          // 2
    { 0.11, 0.10, 0.097, 0.084, 0.067, 0.051, 0.036, 0.0, 0.0 },   // 3 
    { 0.091, 0.09, 0.084, 0.076, 0.066, 0.055, 0.044, 0.034, 0.0}, // 4 
    { 0.063, 0.062, 0.061, 0.059, 0.058, 0.056, 0.054, 0.053, 0.051} // 5
};

void main()
{
    vec2 uvOffset = 1.f / textureSize(srcTexture, 0);
    vec3 result = texture(srcTexture, psIn.texCoord0).rgb * kernels[kernelIndex][0];
    if (horizontal > .5f)
    {
        // horizontal pass
        for (int i = 1; i < radius; ++i)
        {
            result += texture(srcTexture, psIn.texCoord0 + vec2(i * uvOffset.x, 0.f)).rgb * kernels[kernelIndex][i];
            result += texture(srcTexture, psIn.texCoord0 - vec2(i * uvOffset.x, 0.f)).rgb * kernels[kernelIndex][i];
        }
    }
    else
    {
        // vertical pass
        for (int i = 1; i < radius; ++i)
        {
            result += texture(srcTexture, psIn.texCoord0 + vec2(0.f, i * uvOffset.y)).rgb * kernels[kernelIndex][i];
            result += texture(srcTexture, psIn.texCoord0 - vec2(0.f, i * uvOffset.y)).rgb * kernels[kernelIndex][i];
        }
    }
    fragColor = vec4(result, 1.f);
    singleChannelColor = result.r;
}
#else

#define MAX_KERNEL_RADIUS 10

out vec4 outColor;

uniform float pass;
uniform float weights[MAX_KERNEL_RADIUS];
uniform uint kernelRadius;

void main()
{
    vec2 uvOffset = 1.f / textureSize(srcTexture, 0);
    vec3 result = texture(srcTexture, psIn.texCoord0).rgb * weights[0];
    if (pass > 0.5f)
    {
        // horizontal pass
        for (uint i = 1; i < kernelRadius; ++i)
        {
            result += texture(srcTexture, psIn.texCoord0 + vec2(i * uvOffset.x, 0.f)).rgb * weights[i];
            result += texture(srcTexture, psIn.texCoord0 - vec2(i * uvOffset.x, 0.f)).rgb * weights[i];
        }
    }
    else
    {
        // vertical pass
        for (uint i = 1; i < kernelRadius; ++i)
        {
            result += texture(srcTexture, psIn.texCoord0 + vec2(0.f, i * uvOffset.y)).rgb * weights[i];
            result += texture(srcTexture, psIn.texCoord0 - vec2(0.f, i * uvOffset.y)).rgb * weights[i];
        }
    }
    outColor = vec4(result, 1.f);
}

#endif