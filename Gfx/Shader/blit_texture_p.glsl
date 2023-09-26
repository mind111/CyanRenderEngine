#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec4 outColor;

uniform int mipLevel;
uniform sampler2D srcTexture;

void main()
{
	ivec2 texCoord = ivec2(floor(gl_FragCoord.xy));
    outColor = textureLod(srcTexture, psIn.texCoord0, mipLevel).rgba;
}