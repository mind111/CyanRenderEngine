#version 450 core

uniform int srcMip;
uniform sampler2D srcDepthTexture;

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outColor;

void main()
{	
#if 1
	ivec2 texCoord = ivec2(floor(gl_FragCoord.xy)) * 2;
	float minDepth = texelFetch(srcDepthTexture, texCoord, srcMip).r;
	minDepth = min(minDepth, texelFetch(srcDepthTexture, texCoord + ivec2(1, 0), srcMip).r);
	minDepth = min(minDepth, texelFetch(srcDepthTexture, texCoord + ivec2(1, 1), srcMip).r);
	minDepth = min(minDepth, texelFetch(srcDepthTexture, texCoord + ivec2(0, 1), srcMip).r);
#else
	float texelSize = 1.f / textureSize(srcDepthTexture, srcMip).x;
	float minDepth = textureLod(srcDepthTexture, psIn.texCoord0 + vec2(-.5f, .5f) * texelSize, srcMip).r;
	minDepth = min(minDepth, textureLod(srcDepthTexture, psIn.texCoord0 + vec2(.5f, .5f) * texelSize, srcMip).r);
	minDepth = min(minDepth, textureLod(srcDepthTexture, psIn.texCoord0 + vec2(-.5f, -.5f) * texelSize, srcMip).r);
	minDepth = min(minDepth, textureLod(srcDepthTexture, psIn.texCoord0 + vec2(.5f, -.5f) * texelSize, srcMip).r);
#endif
	outColor = vec3(minDepth);
}
