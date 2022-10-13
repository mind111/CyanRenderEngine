#version 450 core

#extension GL_ARB_shader_draw_parameters : enable 
#extension GL_ARB_gpu_shader_int64 : enable 

layout (invocations = 4, triangles) in;
/** note - @min:
* each geometry shader invocation corresponds to rendering one triangle to a hemicube,
* within each invocation, one triangle is rendered to 5 faces of a hemicube, thus invotations = 16,
* and max_vertices = 18 (3 * 6)
*/
layout(triangle_strip, max_vertices = 18) out;

struct PBRMaterial
{
	uint64_t diffuseMap;
	uint64_t normalMap;
	uint64_t metallicRoughnessMap;
	uint64_t occlusionMap;
	vec4 kAlbedo;
	vec4 kMetallicRoughness;
	uvec4 flags;
};

in VertexData {
	vec3 viewSpacePosition;
	vec3 worldSpacePosition;
	vec3 worldSpaceNormal;
	vec3 worldSpaceTangent;
	flat float tangentSpaceHandedness;
	vec2 texCoord0;
	flat PBRMaterial material;
} gsIn[];

out VertexData {	
	vec3 viewSpacePosition;
	vec3 worldSpacePosition;
	vec3 worldSpaceNormal;
	vec3 worldSpaceTangent;
	flat float tangentSpaceHandedness;
	vec2 texCoord0;
	flat PBRMaterial material;
	vec3 hemicubeNormal;
	flat mat4 view;
} gsOut;

out int gl_Layer;
out int gl_ViewportIndex; 

layout(std430, binding = 54) buffer ViewBuffer
{
	mat4 views[];
};

layout(std430, binding = 55) buffer HemicubeNormalBuffer
{
	vec4 hemicubeNormals[];
};

uniform int numViews;
uniform mat4 projection;

void main() {
	if (gl_InvocationID < numViews) {
		for (int layer = 0; layer < 6; ++layer) {
			if (layer == 3) {
				continue;
			}
			mat4 view = views[gl_InvocationID * 6 + layer];
			for (int v = 0; v < gsIn.length(); ++v) {
				vec3 viewSpacePosition = (view * vec4(gsIn[v].worldSpacePosition, 1.f)).xyz;
				gl_Position = projection * view * vec4(gsIn[v].worldSpacePosition, 1.f);
				// pass-through other vertex attributes
				gsOut.worldSpacePosition = gsIn[v].worldSpacePosition;
				gsOut.viewSpacePosition = viewSpacePosition;
				gsOut.worldSpaceNormal = gsIn[v].worldSpaceNormal;
				gsOut.worldSpaceTangent = gsIn[v].worldSpaceTangent;
				gsOut.tangentSpaceHandedness = gsIn[v].tangentSpaceHandedness;
				gsOut.texCoord0 = gsIn[v].texCoord0;
				gsOut.material = gsIn[v].material;
				gsOut.hemicubeNormal = hemicubeNormals[gl_InvocationID].xyz;
				gsOut.view = view;
				gl_Layer = layer;
				gl_ViewportIndex = gl_InvocationID;
				EmitVertex();
			}
			EndPrimitive();
		}
	}
}
