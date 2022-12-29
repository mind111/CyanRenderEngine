#version 450 core
uniform sampler2D srcDepthTexture;
in VSOutput
{
	vec2 texCoord0;
} psIn;
out vec3 outColor;
void main()
{
	outColor = vec3(texture(srcDepthTexture, psIn.texCoord0).r);
}
