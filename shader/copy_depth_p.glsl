#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

uniform sampler2D srcDepth;
uniform int mip;

void main()
{
	gl_FragDepth = texture(srcDepth, psIn.texCoord0).r;
}
