#version 450 core

// this is necessary since using seperable program
out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out VSOutput
{
	int primitiveID;
} vsOut;

void main()
{
	vsOut.primitiveID = gl_VertexID;
	gl_Position = vec4(0.f, 0.f, 0.f, 1.f);
}
