#version 450 core
layout (location = 0) in vec3 vPos; 
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec4 vTangent;
uniform mat4 s_model;
uniform mat4 s_view;
uniform mat4 s_projection;

out vec3 normalWorld;
out vec3 tangentWorld;

void main()
{
    normalWorld = (inverse(transpose(s_model)) * vec4(vNormal, 0.f)).xyz;
    // todo: handedness of tangent
    tangentWorld = normalize((s_model * vec4(vTangent.xyz, 0.f)).xyz);
    gl_Position = s_projection * s_view * s_model * vec4(vPos, 1.f);
}