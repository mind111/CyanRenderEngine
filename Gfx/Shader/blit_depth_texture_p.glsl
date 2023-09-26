#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

uniform sampler2D srcDepthTexture;

void main()
{
	ivec2 texCoordi = ivec2(floor(gl_FragCoord.xy));
    gl_FragDepth = texelFetch(srcDepthTexture, texCoordi, 0).r;
}
