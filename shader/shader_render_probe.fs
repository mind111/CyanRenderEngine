#version 450 core
uniform samplerCube radianceMap;
uniform samplerCube irradianceMap;
in vec3 normal;
out vec4 fragColor;
void main()
{
    fragColor = vec4(textureLod(radianceMap, normalize(normal), 0).rgb, 1.f);
}