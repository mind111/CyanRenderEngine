#version 450 core

in VSOut
{
	vec2 texCoord0;
} psIn;

out vec4 fragColor;

uniform sampler2D srcTexture;

void main()
{
    fragColor = vec4(texture(srcTexture, psIn.texCoord0).rgb, 1.0f);
}