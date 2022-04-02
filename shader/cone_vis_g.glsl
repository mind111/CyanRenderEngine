#version 450 core
/*
* Each geometry shader invocation is responsible for generating vertices for one cube
*/

layout (points) in;
layout (triangle_strip, max_vertices = 36) out;

in VS_OUT
{ 
    float size;
    vec3 color;
} gsIn[];

#define pi 3.1415926

out vec3 voxelColor;

//- buffers
layout(std430, binding = 0) buffer GlobalDrawData
{
    mat4  view;
    mat4  projection;
	mat4  sunLightView;
	mat4  sunShadowProjections[4];
    int   numDirLights;
    int   numPointLights;
    float m_ssao;
    float dummy;
} gDrawData;

uniform float boost;

layout (binding = 10) uniform sampler3D sceneVoxelGridAlbedo;
layout (binding = 11) uniform sampler3D sceneVoxelGridNormal;
layout (binding = 12) uniform sampler3D sceneVoxelGridRadiance;

float cubeVertices[] = {
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

void generateCube(vec3 p, float size, vec3 color)
{
	const int numFaces = 6;
	const int numTriPerFace = 2;
	for (int f = 0; f < numFaces; ++f)
	{
		for (int t = 0; t < numTriPerFace; ++t)
		{
			int triIndex = f * numTriPerFace + t; 
			for (int v = 0; v < 3; ++v)
			{
				float x = cubeVertices[(triIndex * 3 + v) * 3 + 0];
				float y = cubeVertices[(triIndex * 3 + v) * 3 + 1];
				float z = cubeVertices[(triIndex * 3 + v) * 3 + 2];
				gl_Position = gDrawData.projection * gDrawData.view * vec4(p + vec3(x, y, z) * .5f * size, 1.f);
                voxelColor = color;
				EmitVertex();
			}
			EndPrimitive();
		}
	}
}

void visualizeConeStep()
{
	generateCube(gl_in[0].gl_Position.xyz, gsIn[0].size, gsIn[0].color.rgb);
}

void main()
{
    visualizeConeStep();
}