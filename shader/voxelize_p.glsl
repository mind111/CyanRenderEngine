#version 450 core
#extension GL_NV_shader_atomic_float : require

layout(binding = 0, r32ui) uniform coherent volatile uimage3D voxelGridAlbedo;
layout(binding = 1, r32ui) uniform coherent volatile uimage3D voxelGridNormal;
layout(binding = 2, r32ui) uniform coherent volatile uimage3D voxelGridEmission;
layout(binding = 3, r32ui) uniform coherent volatile uimage3D voxelGridRadiance;
layout(binding = 4, r32f)  uniform image3D voxelGridOpacity;

//- sun shadow
layout (binding = 6) uniform sampler2D   shadowCascades[4];

in VertexData
{
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 texCoords;
    vec3 fragmentWorldPos;
} psIn;

out vec4 debugColor;

struct DirLight
{
    vec4 color;
    vec4 direction;
};

struct PointLight
{
    vec4 color;
    vec4 position;
};

//- buffers
layout(std430, binding = 0) buffer GlobalDrawData
{
    mat4  view;
    mat4  projection;
	mat4  sunLightView;
	mat4  sunShadowProjections[4];
    int   numDirLights;
    int   numPointLights;
    float m_ssao;
    float dummy;
} gDrawData;

layout(std430, binding = 1) buffer dirLightsData
{
    DirLight lights[];
} dirLightsBuffer;

layout(std430, binding = 2) buffer pointLightsData
{
    PointLight lights[];
} pointLightsBuffer;

layout(std430) buffer OpacityData
{
    int masks[];
} opacityBuffer;

layout(std430) buffer TexcoordData
{
    ivec4 coords[];
};

layout(binding = 0, offset = 0) uniform atomic_uint counter;

uniform uint matlFlag;
uniform vec4 flatColor;
uniform sampler2D albedoTexture;
uniform sampler2D normalTexture;
uniform vec3 aabbMin;
uniform vec3 aabbMax;

const uint kHasDiffuseMap           = 1 << 4;
const uint kHasNormalMap            = 1 << 5;

vec4 orthoProjection(vec4 v, float l, float r, float b, float t, float near, float far)
{
    vec4 result = vec4(0., 0.f, 0.f, 1.f);
    result.x = 2.0 * (v.x - l) / (r - l) - 1.0;
    result.y = 2.0 * (v.y - b) / (t - b) - 1.0;
    result.z = (v.z - near) / (far - near);
    return result;
} 

/*
    this assumes that every component of v is within [0.0, 1.0]
    @Note: notice that the bits layout of resulting RGBA is wwzzyyxx
*/
uint convertVec4ToRGBA8(vec4 v)
{
    uint result = 0u;
    v *= 255.f;
    result |= (uint(v.w) & 0x000000FF) << 24;
    result |= (uint(v.z) & 0x000000FF) << 16;
    result |= (uint(v.y) & 0x000000FF) << 8;
    result |= (uint(v.x) & 0x000000FF);
    return result;
}

vec4 convertRGBA8ToVec4(uint v)
{
    uint rui = (v & 0x000000FF);
    uint gui = (v & 0x0000FF00) >> 8;
    uint bui = (v & 0x00FF0000) >> 16;
    uint aui = (v & 0xFF000000) >> 24;
    return vec4(float(rui) / 255.f, float(gui) / 255.f, float(bui) / 255.f, float(aui) / 255.f);
}

vec3 encodeHDR(vec3 color)
{
	return color / (color + vec3(1.f));
}

vec3 decodeHDR(vec3 color)
{
    return color / (vec3(1.f) - color);
}

// macro equivalent of the function below
#define IMAGE_ATOMIC_ADD_VEC4(volumeTexture, coord, v)                                                    \
{                                                                                                         \
    uint prev = 0;                                                                                        \
    vec4 partialSum = v;                                                                                  \
    uint partialSumui = convertVec4ToRGBA8(partialSum);                                                   \
    uint currentVal = imageAtomicCompSwap(volumeTexture, coord, prev, partialSumui);                  \
    while(currentVal != prev)                                                                             \
    {                                                                                                     \
        prev = currentVal;                                                                                \
        vec4 valVec4 = convertRGBA8ToVec4(currentVal);                                                    \
        partialSum = vec4(valVec4.xyz * valVec4.w, valVec4.w) + v;                                        \
        partialSum.xyz /= partialSum.w;                                                                   \
        currentVal = imageAtomicCompSwap(volumeTexture, coord, prev, convertVec4ToRGBA8(partialSum)); \
    }                                                                                                     \
}                                                                                                         \

// for some unknown reasons, the following function signature doesn't compile anymore
#if 0
// atomicAdd emulation listed in https://www.seas.upenn.edu/~pcozzi/OpenGLInsights/OpenGLInsights-SparseVoxelization.pdf
/*
    @vec4 v: must be noramlized to [0.f, 1.f]
*/
void imageAtomicAddVec4(layout(r32ui) uimage3D volumeTexture, ivec3 texCoords, vec4 v)
{
    uint prev = 0;
    vec4 partialSum = v;
    uint partialSumui = convertVec4ToRGBA8(partialSum);
    uint currentVal = imageAtomicCompSwap(volumeTexture, texCoords, prev, partialSumui);

    while(currentVal != prev)
    {
        prev = currentVal;
        vec4 valVec4 = convertRGBA8ToVec4(currentVal);
        partialSum = vec4(valVec4.xyz * valVec4.w, valVec4.w) + v;
        partialSum.xyz /= partialSum.w;
        currentVal = imageAtomicCompSwap(volumeTexture, texCoords, prev, convertVec4ToRGBA8(partialSum));
    }
}
#endif

//- sun shadow (duplicated from pbs_p.glsl)
float cascadeIntervals[4] = {0.1f, 0.3f, 0.6f, 1.0f};

struct CascadeOffset
{
    int cascadeIndex;
    float blend;
};

CascadeOffset computeCascadeIndex()
{
    // determine which cascade to sample
    float t = (abs(psIn.fragmentWorldPos.z) - 0.5f) / (100.f - 0.5f);
    int cascadeIndex = 0;
    float blend = 1.f;
    for (int i = 0; i < 4; ++i)
    {
        if (t < cascadeIntervals[i])
        {
            cascadeIndex = i;
            if (i > 0)
            {
                float cascadeRelDepth = (t - cascadeIntervals[i-1]) / (cascadeIntervals[i] - cascadeIntervals[i-1]); 
                cascadeRelDepth = clamp(cascadeRelDepth, 0.f, 0.2f);
                blend = cascadeRelDepth * 5.f;
            }
            break;
        }
    }
    return CascadeOffset(cascadeIndex, blend);
}

float slopeBasedBias(vec3 n, vec3 l)
{
    float cosAlpha = max(dot(n, l), 0.f);
    float tanAlpha = tan(acos(cosAlpha));
    float bias = clamp(tanAlpha * 0.0001f, 0.f, 1.f);
    return bias;
}

float constantBias()
{
    return 0.0025f;
}

float PCFShadow(CascadeOffset cascadeOffset, float bias)
{
    float shadow = 1.f;

    // clip uv used to sample the shadow map 
    vec2 texelOffset = vec2(1.f) / textureSize(shadowCascades[cascadeOffset.cascadeIndex], 0);
    // compute shadowmap uv
    vec4 lightViewFragPos = gDrawData.sunShadowProjections[cascadeOffset.cascadeIndex] * gDrawData.sunLightView * vec4(psIn.fragmentWorldPos, 1.f);
    vec2 shadowTexCoord = floor((lightViewFragPos.xy * .5f + .5f) * textureSize(shadowCascades[cascadeOffset.cascadeIndex], 0)) + .5f; 
    shadowTexCoord /= textureSize(shadowCascades[cascadeOffset.cascadeIndex], 0);

    // bring depth into [0, 1] from [-1, 1]
    float fragmentDepth = lightViewFragPos.z * 0.5f + .5f;

    const int kernelRadius = 2;
    // 5 x 5 filter kernel
    float gaussianKernel[25] = { 
        1.f, 4.f,   7.f, 4.f, 1.f,
        4.f, 16.f, 26.f, 16.f, 4.f,
        7.f, 26.f, 41.f, 26.f, 7.f,
        4.f, 16.f, 26.f, 16.f, 4.f,
        1.f, 4.f,   7.f, 4.f, 1.f
    };
    float convolved = 0.f;
    for (int i = -kernelRadius; i <= kernelRadius; ++i)
    {
        for (int j = -kernelRadius; j <= kernelRadius; ++j)
        {
            vec2 offset = vec2(i, j) * texelOffset;
            vec2 sampleTexCoord = shadowTexCoord + offset;
            if (sampleTexCoord.x < 0.f 
                || sampleTexCoord.x > 1.f 
                || sampleTexCoord.y < 0.f 
                || sampleTexCoord.y > 1.f) 
                continue;

            float depthTest = texture(shadowCascades[cascadeOffset.cascadeIndex], sampleTexCoord).r < (fragmentDepth - bias) ? 0.f : 1.f;
            convolved += depthTest * (gaussianKernel[(i + kernelRadius) * 5 + (j + kernelRadius)] * (1.f / 273.f));
        }
    }
    shadow = convolved;
    return shadow;
}

float isInShadow(float bias)
{
    float shadow = 1.f;
    // determine which cascade to sample
    CascadeOffset cascadeOffset = computeCascadeIndex();
	shadow = PCFShadow(cascadeOffset, bias);
    return shadow;
}

void debugWriteOpacityMask(uvec3 texCoordi, uvec3 gridDim, uint superSampleScale)
{
    texCoordi *= superSampleScale;
    uvec3 dim = gridDim * superSampleScale;
	for(uint i = 0; i < 4; ++i)
	{
		for (uint j = 0; j < 4; ++j)
		{
			for (uint k = 0; k < 4; ++k)
			{
                uvec3 coord = texCoordi + uvec3(k, j, i);
				opacityBuffer.masks[dim.x * dim.y * coord.z + dim.x * coord.y + coord.x] = 1;
			}
		}
	}
}

void writeOpacityMask(uvec3 texCoordi, uvec3 gridDim, uint superSampleScale)
{
    uvec3 dim = gridDim * superSampleScale;
	opacityBuffer.masks[dim.x * dim.y * texCoordi.z + dim.x * texCoordi.y + texCoordi.x] = 1;
}

void main()
{
    ivec3 voxelGridDim = imageSize(voxelGridAlbedo);

    // compute which voxel current fragment corresponds to
    // this coordinates computation doesn't care about whether we are doing super-sampling or not
    vec4 texCoords = orthoProjection(vec4(psIn.fragmentWorldPos, 1.f), aabbMin.x, aabbMax.x, aabbMin.y, aabbMax.y, aabbMax.z, aabbMin.z);
    vec3 normalizedTexCoords = vec3((texCoords.xy + 1.f) * .5f, texCoords.z);
    ivec3 texCoordsi = ivec3(normalizedTexCoords * voxelGridDim);

    // TODO: gamma correct from sRGB to linear
    vec4 albedo = vec4(flatColor.rgb, 1.f); 
    if ((matlFlag & kHasDiffuseMap) != 0)
    {
		albedo.rgb = texture(albedoTexture, psIn.texCoords).rgb;
	}
    // convert normal to [0.0, 1.0] for visualizing;
    vec4 ncolor = vec4((normalize(psIn.normal) + vec3(1.f)) * 0.5f, 1.f); 

    // accumulate relevant data into voxel fragment
    // albedo
    // imageAtomicAddVec4(voxelGridAlbedo, texCoordsi, vec4(albedo, 1.0f));
    IMAGE_ATOMIC_ADD_VEC4(voxelGridAlbedo, texCoordsi, albedo)

    // normal
    // imageAtomicAddVec4(voxelGridNormal, texCoordsi, vec4(ncolor, 1.0f));
    // IMAGE_ATOMIC_ADD_VEC4(voxelGridNormal, texCoordsi, ncolor)

    // todo: emission
    // todo: combine opacity & emission into a r32 volume texture
    // super-sampled opactiy
	ivec3 texCoordsiSS = ivec3(vec3((texCoords.xy + 1.f) * .5f, texCoords.z) * voxelGridDim * 4);
    writeOpacityMask(texCoordsiSS, voxelGridDim, 4);

    // inject direct lighting into voxels
    vec4 radiance = vec4(0.f, 0.f, 0.f, 1.f);
    // direct sun light
    for (int i = 0; i < gDrawData.numDirLights; ++i)
    {
        vec3 n = normalize(psIn.normal);
        vec3 l = normalize(dirLightsBuffer.lights[i].direction.xyz);
		float ndotl = max(0.f, dot(n, l));
        float shadowBias = constantBias() + slopeBasedBias(n, l);
        // sun shadow
        float shadow = isInShadow(shadowBias);
		radiance.rgb += shadow * dirLightsBuffer.lights[i].color.rgb * dirLightsBuffer.lights[i].color.a * albedo.rgb * ndotl;
    }
    // todo: direct sky light
    {

    }
	// encode hdr radiance value
	radiance.rgb /= (radiance.rgb + vec3(1.f));

    // diffusely reflected radiance
    // imageAtomicAddVec4(voxelGridRadiance, texCoordsi, vec4(radiance, 1.0f));
    IMAGE_ATOMIC_ADD_VEC4(voxelGridRadiance, texCoordsi, radiance)

    debugColor = albedo;
}