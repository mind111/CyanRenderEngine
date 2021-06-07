#version 450 core

layout (location = 0) in vec3 vertexPos;

out vec3 fragmentObjPos;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    fragmentObjPos = vertexPos;
    mat4 viewRotation = view;
    viewRotation[3][0] = 0.f;
    viewRotation[3][1] = 0.f;
    viewRotation[3][2] = 0.f;
    viewRotation[3][3] = 1.f;
    gl_Position = projection * viewRotation * vec4(vertexPos, 1.f); 
    gl_Position = gl_Position.xyww;
}