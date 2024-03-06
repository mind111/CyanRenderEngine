#version 450 core

in VertexData
{
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 texCoord;
    vec3 albedo;
    float roughness;
    float metallic;
} psIn;

layout (binding = 0) uniform atomic_uint u_voxelFragmentCounter;

void main()
{
    atomicCounterIncrement(u_voxelFragmentCounter);
}
