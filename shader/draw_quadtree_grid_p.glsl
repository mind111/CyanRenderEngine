#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outColor;

uniform sampler2D backgroundTexture;
uniform sampler2D srcQuadtreeTexture;
uniform int mip;

void main()
{
	outColor = texture(backgroundTexture, psIn.texCoord0).rgb;

	float numCells = textureSize(srcQuadtreeTexture, mip).x;
	float cellSize = 1.f / numCells;
	float l = floor(psIn.texCoord0.x / cellSize) * cellSize;
	float r = ceil(psIn.texCoord0.x / cellSize) * cellSize;
	float b = floor(psIn.texCoord0.y / cellSize) * cellSize;
	float t = ceil(psIn.texCoord0.y / cellSize) * cellSize;

	float d = abs(psIn.texCoord0.x - l);
	d = min(d, abs(psIn.texCoord0.x - r));
	d = min(d, abs(psIn.texCoord0.y - b));
	d = min(d, abs(psIn.texCoord0.y - t));
	
	float gridLineWidth = cellSize * 0.05f;
	if (d < gridLineWidth)
	{
		outColor = vec3(1.f, 0.f, 0.f);
	}
}
