#version 450 core

#define DEBUG 0

layout (points) in;
#if DEBUG 
	layout (points, max_vertices = 1) out;
#else
	layout (triangle_strip, max_vertices = 36) out;
#endif

in VsOut
{
	vec3 voxelCenterPos;
    vec4 voxelColor;
} gsIn[];

out vec4 voxelColor;

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

//- voxel cone tracing
layout(std430, binding = 4) buffer VoxelGridData
{
    vec3 localOrigin;
    float voxelSize;
    int visMode;
    vec3 padding;
} sceneVoxelGrid;

uniform int activeMip;

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

void main()
{
	// generate a cube for each voxel center
#if DEBUG
	gl_Position = gDrawData.projection * gDrawData.view * vec4(gsIn[0].voxelCenterPos, 1.f);
	voxelColor = gsIn[0].voxelColor.rgb;
	EmitVertex();
	EndPrimitive();
#else
	if (gsIn[0].voxelColor.a > 0.f)
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
					gl_Position = gDrawData.projection * gDrawData.view * vec4(gsIn[0].voxelCenterPos + vec3(x, y, z) * .5f * sceneVoxelGrid.voxelSize * pow(2.f, activeMip), 1.f);
					voxelColor = gsIn[0].voxelColor;
					EmitVertex();
				}
				EndPrimitive();
			}
		}
	}
#endif
}