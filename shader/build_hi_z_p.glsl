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
	ivec2 texCoord = ivec2(gl_FragCoord.xy) * 2;
	float minDepth = texelFetch(srcDepthTexture, texCoord, srcMip).r;
	minDepth = min(minDepth, texelFetch(srcDepthTexture, texCoord + ivec2(1, 0), srcMip).r);
	minDepth = min(minDepth, texelFetch(srcDepthTexture, texCoord + ivec2(1, 1), srcMip).r);
	minDepth = min(minDepth, texelFetch(srcDepthTexture, texCoord + ivec2(0, 1), srcMip).r);
	outColor = vec3(minDepth);
}
