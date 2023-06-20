#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

uniform sampler2D sceneDepthBuffer;

out float outDepth;

void main()
{
	outDepth = texture(sceneDepthBuffer, psIn.texCoord0).r;
}
