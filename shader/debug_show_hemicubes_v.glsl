#version 450 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 tangent;
layout (location = 3) in vec2 texCoord0;
layout (location = 4) in vec2 texCoord1;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out VSOutput
{
    flat int instanceID;
    vec3 objectSpacePosition;
    flat ivec2 atlasTexCoord;
} vsOut;

layout(std430) buffer ViewBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
};

struct InstanceDesc 
{
    mat4 transform;
    ivec2 atlasTexCoord;
    ivec2 padding;
};

layout(std430) buffer HemicubeInstanceBuffer 
{
    InstanceDesc instances[];
};

void main() 
{
	mat4 model = instances[gl_InstanceID].transform;
    mat4 mvp = projection * view * model;
	gl_Position = mvp * vec4(position, 1.f);
    vsOut.instanceID = gl_InstanceID;
    vsOut.objectSpacePosition = position;
    vsOut.atlasTexCoord = instances[gl_InstanceID].atlasTexCoord;
}
