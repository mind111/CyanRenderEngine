#version 450 core

layout (location = 0) in vec3 vertexPos;

out vec3 fragmentObjPos;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    fragmentObjPos = vertexPos;
    mat4 viewRotation = view;
    // Cancel out the translation part of the view matrix so that the cubemap will always follow
    // the camera, view of the cubemap will change along with the change of camera rotation 
    viewRotation[3][0] = 0.f;
    viewRotation[3][1] = 0.f;
    viewRotation[3][2] = 0.f;
    viewRotation[3][3] = 1.f;
    vec4 position = projection * viewRotation * vec4(vertexPos, 1.f);
    position.z = position.w;
    gl_Position = position; 
}