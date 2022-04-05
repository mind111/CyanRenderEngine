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

layout(std430) buffer ConeTraceDebugData
{
    int numCubes;
    vec3 padding;
	ConeCube cubes[];
} debugConeBuffer;

void main()
{
	vsOut.size = debugConeBuffer.cubes[gl_VertexID].size;
	vsOut.color = debugConeBuffer.cubes[gl_VertexID].color.rgb;
	gl_Position = vec4(debugConeBuffer.cubes[gl_VertexID].center, 1.f);
}