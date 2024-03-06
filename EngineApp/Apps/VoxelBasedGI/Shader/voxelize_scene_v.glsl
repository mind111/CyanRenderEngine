#version 450 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 tangent;
layout (location = 3) in vec2 texCoord0;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out VertexData
{
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 texCoord;
	vec3 albedo;
	float roughness;
	float metallic;
} vsOut;

uniform mat4 u_localToWorldMatrix;

// not supporting material texturing for now
uniform vec3 u_albedo;
uniform float u_metallic;
uniform float u_roughness;

void main() 
{
	// transform everything to world space and then geometry shader only does pass through
    vec3 worldSpacePosition = (u_localToWorldMatrix * vec4(position, 1.f)).xyz;
    vec3 worldSpaceNormal = normalize((inverse(transpose(u_localToWorldMatrix)) * vec4(normal, 0.f)).xyz);
    vec3 worldSpaceTangent = normalize((u_localToWorldMatrix * vec4(tangent.xyz, 0.f)).xyz); 

    vsOut.position = worldSpacePosition;
    vsOut.normal = worldSpaceNormal;
    vsOut.tangent = vec4(worldSpaceTangent, tangent.w);
    vsOut.texCoord = texCoord0;
    vsOut.albedo = u_albedo;
    vsOut.roughness = u_roughness;
    vsOut.metallic = u_metallic;

    gl_Position = vec4(worldSpacePosition, 1.f);
}
