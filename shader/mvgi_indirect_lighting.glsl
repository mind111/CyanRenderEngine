#version 450 core

uniform sampler2D irradiance;
uniform sampler2D mvgiAlbedo;

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outColor;

void main()
{
	outColor = texture(irradiance, psIn.texCoord0).rgb * texture(mvgiAlbedo, psIn.texCoord0).rgb;
}