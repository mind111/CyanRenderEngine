#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;
out vec3 color;

uniform sampler2D srcTexture; 

void main() 
{
	color = texture(srcTexture, psIn.texCoord0).rgb; 
}
