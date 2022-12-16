#version 450 core

layout (location = 0) in vec3 position;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};
out vec3 objectSpacePosition;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    objectSpacePosition = position;
    mat4 viewRotation = view;
    // Cancel out the translation part of the view matrix so that the cubemap will always follow
    // the camera, view of the cubemap will change along with the change of camera rotation 
    viewRotation[3][0] = 0.f;
    viewRotation[3][1] = 0.f;
    viewRotation[3][2] = 0.f;
    viewRotation[3][3] = 1.f;
    gl_Position = projection * viewRotation * vec4(position, 1.f); 
}