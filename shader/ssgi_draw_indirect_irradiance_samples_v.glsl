#version 450 core

struct Sample
{
	vec4 position;
	vec4 radiance;
};

layout (std430) buffer SampleBuffer
{
	Sample samples[];
};

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out VSOut
{
	vec4 color;
} vsOut;

void main()
{	
	Sample s = samples[gl_VertexID];
	vsOut.color = s.radiance;
	gl_Position = vec4(s.position.xy * 2.f - 1.f, 0.f, 1.f);
	gl_PointSize = 5.f;
}