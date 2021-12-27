#version 450 core

/*
    need to use multiple textures because the atomic image operation used when aggregating
    data for each voxel fragment only works for 32bits images. With this limitation, each component
    only has 8 bits precision, will this become a problem later...?
*/

// may need to use multiple uimage3D
// layout(binding = 0, rgba16f) uniform coherent volatile image3D voxelVolumeTexture; 
layout(binding = 0, r32ui) uniform coherent volatile uimage3D voxelAlbedoTexture;
layout(binding = 1, r32ui) uniform coherent volatile uimage3D voxelNormalTexture;
layout(binding = 2, r32ui) uniform coherent volatile uimage3D voxelEmissionTexture;

// atomic counter
layout(binding = 0) uniform atomic_uint voxelFragmentCounter;

struct VoxelFragment
{
    vec3 albedo;
    vec3 normal;
};

/*
// voxel fragment list
layout(std430, binding = 0) buffer VoxelFragmentData
{
    VoxelFragment m_voxelFragments[];
} voxelFragmentList;
*/

in VertexData
{
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 texCoords;
    vec3 fragmentWorldPos;
} VertexIn;

out vec4 fragColor;

uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform vec3 aabbMin;
uniform vec3 aabbMax;

/*
// atomicAdd emulation listed in https://www.seas.upenn.edu/~pcozzi/OpenGLInsights/OpenGLInsights-SparseVoxelization.pdf
void imageAtomicAddF32(coherent volatile uimage3D volumeTexture, ivec3 texCoords, float value)
{
    uint prev = 0;
    uint partialSum = floatBitsToUint(value);
    uint currentVal = imageAtomicCompSwap(voxelVolumeTexture, texCoords, prev, partialSum);
    while(curretVal != prev)
    {
        prev = currentVal;
        partialSum += floatBitsToUint(value + uintBitsToFloat(partialSum));
        currentVal = imageAtomicCompSwap(volumeTexture, texCoords, prev, partialSum);
    }
}
*/

vec4 orthoProjection(vec4 v, float l, float r, float b, float t, float near, float far, float aspect)
{
    float asp = (r - l) / (t - b);
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
    result |= (uint(v.w)&0x000000FF) << 24;
    result |= (uint(v.z)&0x000000FF) << 16;
    result |= (uint(v.y)&0x000000FF) << 8;
    result |= (uint(v.x)&0x000000FF);
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

// atomicAdd emulation listed in https://www.seas.upenn.edu/~pcozzi/OpenGLInsights/OpenGLInsights-SparseVoxelization.pdf
/*
    @vec4 v: must be noramlized to [0.f, 1.f]
*/
void imageAtomicAddVec4(layout(r32ui) coherent volatile uimage3D volumeTexture, ivec3 texCoords, vec4 v)
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

void main()
{
    ivec3 voxelGridDim = imageSize(voxelNormalTexture);
    float ndotl = max(dot(vec3(0.f, 1.f, 0.f), VertexIn.normal), 0.0); 
    vec3 color = (VertexIn.normal + vec3(1.f)) * 0.5f;

    // compute which voxel current fragment corresponds to
    // ivec3 as uv so no need to normalize it to [0, 1]
    // int x = int(gl_FragCoord.x);
    // int y = int(gl_FragCoord.y);
    // int z = int(gl_FragCoord.z * voxelGridDim.z);
    // ivec3 texCoords = ivec3(x, y, z);
    float left = aabbMin.x;
    float right = aabbMax.x;
    float bottom = aabbMin.y;
    float top = aabbMax.y;
    float near = aabbMax.z;
    float far = aabbMin.z - 0.01f;
    vec4 aabbCoords = orthoProjection(vec4(VertexIn.fragmentWorldPos, 1.f), left, right, bottom, top, near, far, 1.0f);
    vec3 normalizedAabbCoords = vec3((aabbCoords.xy + vec2(1.0)) * .5f, aabbCoords.z); 
    vec3 tempTexCoord = normalizedAabbCoords * voxelGridDim; 
    ivec3 texCoords = ivec3(tempTexCoord.x, tempTexCoord.y, tempTexCoord.z);

    // TODO: gamma correct from sRGB to linear
    vec3 albedo = texture(albedoMap, VertexIn.texCoords).rgb;
    // convert normal to [0.0, 1.0];
    vec3 n = (normalize(VertexIn.normal) + vec3(1.f)) * 0.5f; 

    // accumulate relevant data into voxel fragment
    // albedo
    imageAtomicAddVec4(voxelAlbedoTexture, texCoords, vec4(albedo, 1.0f));
    // normal
    imageAtomicAddVec4(voxelNormalTexture, texCoords, vec4(n, 1.0f));
    // TODO: emission

/* TODO: implement sparse voxel octree
    VoxelFragment voxel = VoxelFragment(albedo, n.xyz);
    uint index = atomicCounterIncrement(voxelFragmentCounter); 
    // push the voxel fragment into the voxel fragment list

    @Note(Min): note that multiple voxel fragments with the same voxel position will exsit
                in the list at the same time becasue multiple triangles can overlap the same
                voxel fragment during rasterization pass. I guess we do the average later when 
                writing the voxel data into leaf nodes of sparse voxel octree. 
    voxelFragmentList.m_voxelFragments[index] = voxel;
*/
    fragColor = vec4(color, 1.f);
}