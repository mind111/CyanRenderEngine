#version 450 core

in VSOut 
{
	vec2 texCoord0;
} psIn;

layout (location = 0) out vec3 outReservoirRadiance;
layout (location = 1) out vec3 outReservoirSamplePosition;
layout (location = 2) out vec3 outReservoirSampleNormal;
layout (location = 3) out vec3 outReservoirWSumMW;

uniform sampler2D temporalReservoirRadiance;
uniform sampler2D temporalReservoirSamplePosition;
uniform sampler2D temporalReservoirSampleNormal;
uniform sampler2D temporalReservoirWSumMW;

void main()
{
	outReservoirRadiance = texture(temporalReservoirRadiance, psIn.texCoord0).rgb;
	outReservoirSamplePosition = texture(temporalReservoirSamplePosition, psIn.texCoord0).xyz;
	outReservoirSampleNormal = texture(temporalReservoirSampleNormal, psIn.texCoord0).xyz;
	outReservoirWSumMW = texture(temporalReservoirWSumMW, psIn.texCoord0).xyz;
}
