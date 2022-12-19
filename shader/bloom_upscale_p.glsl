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
    // additive blending
    outColor = vec4(upscaledColor + blendColor, 1.f);
}