#version 450 core 
in vec3 samplePos;
out vec4 fragmentColor;

uniform samplerCube cubemapSampler;

void main() {
    fragmentColor = texture(cubemapSampler, samplePos);
}