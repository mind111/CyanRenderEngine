#version 450 core

layout (location = 0) in vec3 position;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out VSOutput
{
	vec3 objectSpacePosition;
} vsOut;

uniform mat4 cameraView;
uniform mat4 cameraProjection;

void main()
{
    vsOut.objectSpacePosition = position;
    mat4 view = cameraView;
    // remove translation from view matrix
    view[3] = vec4(0.f, 0.f, 0.f, 1.f);
    gl_Position = (cameraProjection * view * vec4(position, 1.f)).xyww;
}