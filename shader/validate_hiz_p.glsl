#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outError;

uniform sampler2D sceneDepthBuffer;
uniform sampler2D depthQuadtree;

void main()
{
	outError = vec3(0.f, 1.f, 0.f);
	float sceneDepth = texture(sceneDepthBuffer, psIn.texCoord0).r;
	float resampledDepth = textureLod(depthQuadtree, psIn.texCoord0, 0).r;
	if (sceneDepth < resampledDepth)
	{
		outError = vec3(1.f, 0.f, 0.f);
	}
}
