#version 450 core
layout (location = 0) out vec3 outDepth;
layout (location = 1) out vec3 outNormal;

in vec3 worldSpaceNormal; 
in vec3 worldSpaceTangent;

void main() {
    outDepth = vec3(gl_FragCoord.z);
    outNormal = normalize(worldSpaceNormal) * .5f + .5f;
}