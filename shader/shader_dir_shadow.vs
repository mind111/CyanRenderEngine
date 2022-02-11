#version 450 core
layout(location = 0) in vec3 vPos;
out vec3 vPosCS; 

layout(std430, binding = 3) buffer InstanceTransformData
{
    mat4 models[];
} gInstanceTransforms;

uniform mat4 sunLightView;
uniform mat4 sunLightProjection;
uniform int transformIndex;

void main()
{
    mat4 model = gInstanceTransforms.models[transformIndex];
    vPosCS = (sunLightProjection * sunLightView * model * vec4(vPos, 1.f)).xyz;
    gl_Position = vec4(vPosCS, 1.f);
}
