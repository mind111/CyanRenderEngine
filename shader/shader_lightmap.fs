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
    // if (pass > 3.5f)
    // {

    uint index = uint(floor(gl_FragCoord.y)) * uint(textureSize.x) + uint(floor(gl_FragCoord.x));
    if (bakeTexelBuffer.m_bakeTexels[index].texCoord.w < .5f) atomicCounterIncrement(texelCounter);
    bakeTexelBuffer.m_bakeTexels[index].position = vec4(worldPos, 0.f);
    bakeTexelBuffer.m_bakeTexels[index].normal = vec4(normalize(normal), 0.f);
    bakeTexelBuffer.m_bakeTexels[index].texCoord = vec4(gl_FragCoord.xy, 0.f, 1.f);

    // }
    // float sign = sign(sin(100.f * texCoord.x * 2 * pi) * sin(100.f * texCoord.y * 2 * pi));
    // vec3 color = sign < 0.f ? vec3(0.1f) : vec3(0.8f);
    // fragColor = vec4(color, 1.f);
}