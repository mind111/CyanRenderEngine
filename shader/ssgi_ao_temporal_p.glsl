#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

uniform sampler2D currentFrameAOBuffer;
uniform sampler2D AOHistoryBuffer;
out vec3 outAO;

void main()
{
	vec2 texCoord = psIn.texCoord0;
    outAO = vec3(.9f * texture(AOHistoryBuffer, texCoord).r + .1f * texture(currentFrameAOBuffer, texCoord).r);
}
