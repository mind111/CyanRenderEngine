#version 450 core

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

const float quadVerts[18] = {
	-1.f, -1.f, 0.f, 
	 1.f,  1.f, 0.f,
	-1.f,  1.f, 0.f,
	-1.f, -1.f, 0.f,
	 1.f, -1.f, 0.f,
	 1.f,  1.f, 0.f
};

void main()
{
	vec3 vPos = vec3(quadVerts[gl_VertexID * 3 + 0], quadVerts[gl_VertexID * 3 + 1], quadVerts[gl_VertexID * 3 + 2]);
    gl_Position = vec4(vPos, 1.f);
}
