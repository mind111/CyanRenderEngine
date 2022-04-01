#version 450 core 

out int index;

uniform vec3 debugRayOrigin;

void main()
{
	index = gl_VertexID;
	gl_Position = vec4(debugRayOrigin, 1.f);
}