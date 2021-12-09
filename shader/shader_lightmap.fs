#version 450 core
in vec3 worldPos;
in vec3 normal;
out vec4 fragColor;

#define pi 3.14159265359

struct LightMapTexelData
{
    vec4 position;
    vec4 normal; 
    vec4 texCoord;
};

layout(std430, binding = 0) buffer LightMapData
{
    LightMapTexelData m_bakeTexels[];
} bakeTexelBuffer;

layout(binding = 0) uniform atomic_uint texelCounter;

uniform float pass;
uniform vec3 textureSize;

void main()
{
    // todo: this might be helpful later when we want to do super-sampling
    // vec2 texCoord = vec2(gl_FragCoord.x / textureSize.x, gl_FragCoord.y / textureSize.y);

    // write baking requried data
    uint index = uint(floor(gl_FragCoord.y)) * uint(textureSize.x) + uint(floor(gl_FragCoord.x));
    if (bakeTexelBuffer.m_bakeTexels[index].texCoord.w < .5f) atomicCounterIncrement(texelCounter);
    bakeTexelBuffer.m_bakeTexels[index].position = vec4(worldPos, 0.f);
    bakeTexelBuffer.m_bakeTexels[index].normal = vec4(normalize(normal), 0.f);
    bakeTexelBuffer.m_bakeTexels[index].texCoord = vec4(gl_FragCoord.xy, 0.f, 1.f);
    if (pass != 8.0f) fragColor = vec4(1.0, 0.f, 0.f, 1.f);
    else 
        fragColor = vec4(0.8f, 0.8f, 0.8f, 1.f); 
}