#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;
uniform sampler2D srcTexture;

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
