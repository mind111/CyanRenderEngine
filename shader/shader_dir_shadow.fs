#version 450 core
layout(location = 0) out vec2 moments; 
in vec3 vPosCS;
void main()
{
    float depth = vPosCS.z * .5f + .5f; 
    float moment2 = depth * depth;
    // TODO: bias the second moment ..?
    moments = vec2(depth, moment2);
}