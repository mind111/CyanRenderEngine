#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

uniform sampler2D u_sceneDepthTex;
uniform vec2 u_outputResolution;

out float outDepth;

void main()
{
	vec2 inputTextureSize = textureSize(u_sceneDepthTex, 0).xy;
	vec2 inputTexelSize = 1.f / inputTextureSize;
	vec2 outputTexelSize = 1.f / u_outputResolution;
	// A --- B
	// |     |
	// C --- D
	vec2 texelCornerA = psIn.texCoord0 + vec2(-.5f, .5f) * outputTexelSize;
	vec2 texelCornerB = psIn.texCoord0 + vec2( .5f, .5f) * outputTexelSize;
	vec2 texelCornerC = psIn.texCoord0 + vec2( .5f,-.5f) * outputTexelSize;
	vec2 texelCornerD = psIn.texCoord0 + vec2(-.5f,-.5f) * outputTexelSize;

	// snap output pixel corners to center of input texels
	vec2 footprintCornerA = (floor(texelCornerA / inputTexelSize) + .5f); 
	vec2 footprintCornerB = (floor(texelCornerB / inputTexelSize) + .5f);
	vec2 footprintCornerC = (floor(texelCornerC / inputTexelSize) + .5f);
	vec2 footprintCornerD = (floor(texelCornerD / inputTexelSize) + .5f);

	int numSamplesInX = int(footprintCornerB.x - footprintCornerA.x) + 1;
	int numSamplesInY = int(footprintCornerA.y - footprintCornerC.y) + 1;

	float minDepth = 1.f;
	vec2 topLeftCoord = footprintCornerA / inputTextureSize;
	for (int i = 0; i < numSamplesInY; ++i)
	{
		for (int j = 0; j < numSamplesInX; ++j)
		{
			vec2 sampleCoord = topLeftCoord + vec2(j, -i) * inputTexelSize;
			if (sampleCoord.x >= 0.f && sampleCoord.x <= 1.f && sampleCoord.y >= 0.f && sampleCoord.y <= 1.f)
			{
				minDepth = min(texture(u_sceneDepthTex, sampleCoord).r, minDepth);
			}
		}
	}

	outDepth = minDepth;
}
