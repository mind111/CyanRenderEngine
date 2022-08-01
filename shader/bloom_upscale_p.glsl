#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec4 outColor;

uniform sampler2D srcTexture;
uniform sampler2D blendTexture;

void main()
{
    vec3 upscaledColor = texture(srcTexture, psIn.texCoord0).rgb;
    vec3 blendColor = texture(blendTexture, psIn.texCoord0).rgb;
#if 0
    // linear blending
    outColor = vec4(mix(upscaledColor, blendColor, 0.5f), 1.f);
#else
    // additive blending
    outColor = vec4(upscaledColor + blendColor, 1.f);
#endif
}