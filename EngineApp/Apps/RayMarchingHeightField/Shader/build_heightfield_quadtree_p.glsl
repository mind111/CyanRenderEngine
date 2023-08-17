#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out float outMaxHeight;

uniform sampler2D u_heightFieldQuadTreeTexture;
uniform int u_srcMipLevel; 

void main()
{
	ivec2 pixelCoordi = ivec2(floor(gl_FragCoord.xy));

	// C --- D
	// |  x  | x is the texel center
	// A --- B
	ivec2 coordA = pixelCoordi * 2;
	float heightA = texelFetch(u_heightFieldQuadTreeTexture, coordA, u_srcMipLevel).r;

	ivec2 coordB = coordA + ivec2(1, 0);
	float heightB = texelFetch(u_heightFieldQuadTreeTexture, coordB, u_srcMipLevel).r;

	ivec2 coordC = coordA + ivec2(0, 1);
	float heightC = texelFetch(u_heightFieldQuadTreeTexture, coordC, u_srcMipLevel).r;

	ivec2 coordD = coordA + ivec2(1, 1);
	float heightD = texelFetch(u_heightFieldQuadTreeTexture, coordD, u_srcMipLevel).r;

	outMaxHeight = max(heightA, heightB);
	outMaxHeight = max(outMaxHeight, heightC);
	outMaxHeight = max(outMaxHeight, heightD);
}
