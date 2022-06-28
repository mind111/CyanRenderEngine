#version 450 core

in VSOutput
{
	vec2 texCoord;
} psIn;

out vec4 outColor;

uniform uint numFrames;
uniform float sampleWeight;
uniform sampler2D oldestColorTexture;
uniform sampler2D currentColorTexture;
uniform sampler2D accumulatedColorTexture;

void main()
{
	vec3 currentColor = texture(currentColorTexture, psIn.texCoord).rgb;
	// vec3 currentColor = texture(currentColorTexture, psIn.texCoord).rgb * sampleWeight;
	vec3 accumulatedColor = texture(accumulatedColorTexture, psIn.texCoord).rgb;
#if 0
	outColor = vec4((accumulatedColor * numFrames + currentColor) / (numFrames + 1), 1.f);
#else
/*
	if (numFrames < 8)
	{
		outColor = vec4((accumulatedColor * numFrames + currentColor) / (numFrames + 1), 1.f);
	}
	else
	{
		// moving average
		vec3 oldestColor = texture(oldestColorTexture, psIn.texCoord).rgb;
		accumulatedColor = (accumulatedColor * 8.0 - oldestColor + currentColor) / 8.0;
		outColor = vec4(accumulatedColor, 1.f);
	}
*/
	if (numFrames == 0)
	{
		outColor = vec4(currentColor, 1.f);
	}
	else
	{
		outColor = vec4(mix(currentColor, accumulatedColor, 0.9f), 1.f);
	}
#endif
}
