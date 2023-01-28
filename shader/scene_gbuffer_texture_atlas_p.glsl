#version 450 core

#define TEXTURE_ATLAS_R8 0
#define TEXTURE_ATLAS_RGB8 1
#define TEXTURE_ATLAS_RGBA8 2

struct ImageTransform
{
	vec2 scale;
	vec2 translate;
};

struct Subtexture
{
	int src;
	int wrapS;
	int wrapT;
	int minFilter;
	int magFilter;
	vec3 padding;
};

uniform sampler2D textureAtlas_R8;
layout (std430) buffer ImageTransformBuffer_R8
{
	ImageTransform imageTransforms0[];
};
layout (std430) buffer SubtextureBuffer_R8
{
	Subtexture subtextures0[];
};

uniform sampler2D textureAtlas_RGB8;
layout (std430) buffer ImageTransformBuffer_RGB8
{
	ImageTransform imageTransforms1[];
};
layout (std430) buffer SubtextureBuffer_RGB8
{
	Subtexture subtextures1[];
};

uniform sampler2D textureAtlas_RGBA8;
layout (std430) buffer ImageTransformBuffer_RGBA8
{
	ImageTransform imageTransforms2[];
};
layout (std430) buffer SubtextureBuffer_RGBA8
{
	Subtexture subtextures2[];
};

struct PackedTextureDesc
{
	int atlasIndex;
	int subtextureIndex;
};

struct MaterialTextureAtlas
{
	PackedTextureDesc albedoMap;
	PackedTextureDesc normalMap;
	PackedTextureDesc metallicRoughnessMap;
	PackedTextureDesc occlusionMap;
	vec4 albedo;
	float metallic;
	float roughness;
	float emissive;
	int flag;
};

in VSOutput
{
	vec2 texCoord;
	MaterialTextureAtlas matlDesc;
} psIn;

out vec3 albedo;

vec3 sampleTextureAtlas(in PackedTextureDesc packedTextureDesc, vec2 texCoord)
{
	vec3 outColor = vec3(0.f);
	if (packedTextureDesc.atlasIndex == TEXTURE_ATLAS_R8)
	{
		Subtexture subtexture = subtextures0[packedTextureDesc.subtextureIndex];
		ImageTransform t = imageTransforms0[subtexture.src];
		outColor = texture(textureAtlas_R8, t.scale * texCoord + t.translate).rgb;
		// assuming fixed addressing mode and filtering for now
	}
	else if (packedTextureDesc.atlasIndex == TEXTURE_ATLAS_RGB8)
	{
		Subtexture subtexture = subtextures1[packedTextureDesc.subtextureIndex];
		ImageTransform t = imageTransforms1[subtexture.src];
		outColor = texture(textureAtlas_R8, t.scale * texCoord + t.translate).rgb;
	}
	else if (packedTextureDesc.atlasIndex == TEXTURE_ATLAS_RGBA8)
	{
		Subtexture subtexture = subtextures2[packedTextureDesc.subtextureIndex];
		ImageTransform t = imageTransforms2[subtexture.src];
		outColor = texture(textureAtlas_R8, t.scale * texCoord + t.translate).rgb;
	}
	return outColor;
}

void main()
{
	vec3 albedo = sampleTextureAtlas(psIn.matlDesc.albedoMap, psIn.texCoord);
	vec3 normal = sampleTextureAtlas(psIn.matlDesc.normalMap, psIn.texCoord);
	vec3 metallicRoughness = sampleTextureAtlas(psIn.matlDesc.metallicRoughnessMap, psIn.texCoord);
}
