#version 450 core

uniform int u_srcMip;
uniform sampler2D u_srcDepthTexture;

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outColor;

void main()
{	
#if 1
	ivec2 texCoord = ivec2(floor(gl_FragCoord.xy)) * 2;
	float minDepth = texelFetch(u_srcDepthTexture, texCoord, u_srcMip).r;
	minDepth = min(minDepth, texelFetch(u_srcDepthTexture, texCoord + ivec2(1, 0), u_srcMip).r);
	minDepth = min(minDepth, texelFetch(u_srcDepthTexture, texCoord + ivec2(1, 1), u_srcMip).r);
	minDepth = min(minDepth, texelFetch(u_srcDepthTexture, texCoord + ivec2(0, 1), u_srcMip).r);
#else
	float texelSize = 1.f / textureSize(u_srcDepthTexture, u_srcMip).x;
	float minDepth = textureLod(u_srcDepthTexture, psIn.texCoord0 + vec2(-.5f, .5f) * texelSize, u_srcMip).r;
	minDepth = min(minDepth, textureLod(u_srcDepthTexture, psIn.texCoord0 + vec2(.5f, .5f) * texelSize, u_srcMip).r);
	minDepth = min(minDepth, textureLod(u_srcDepthTexture, psIn.texCoord0 + vec2(-.5f, -.5f) * texelSize, u_srcMip).r);
	minDepth = min(minDepth, textureLod(u_srcDepthTexture, psIn.texCoord0 + vec2(.5f, -.5f) * texelSize, u_srcMip).r);
#endif
	outColor = vec3(minDepth);
}
