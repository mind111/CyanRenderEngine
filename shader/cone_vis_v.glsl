#version 450 core 

out VS_OUT
{
	float size;
	vec3 color;
} vsOut;

struct ConeCube
{
	vec3 center;
	float size;
    vec4 color;
};

layout(std430) buffer ConeDebugData
{
    int numCubes;
    vec3 padding;
	ConeCube cubes[];
} debugConeData;

void main()
{
	vsOut.size = debugConeData.cubes[gl_VertexID].size;
	vsOut.color = debugConeData.cubes[gl_VertexID].color.rgb;
	gl_Position = vec4(debugConeData.cubes[gl_VertexID].center, 1.f);
}