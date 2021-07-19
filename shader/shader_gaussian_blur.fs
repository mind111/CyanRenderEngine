#version 450 core

in vec2 uv;

out vec4 fragColor; 

uniform float horizontal;
uniform sampler2D srcImage; 

// TODO: Increase kernel filter size to produce bigger bloom
/*
    * 5 x 5 gaussian kernel
*/
void main()
{
    float weights[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.0162160 };
    vec2 uvOffset = 1.f / textureSize(srcImage, 0);
    vec3 result = texture(srcImage, uv).rgb * weights[0];
    if (horizontal > .5f)
    {
        // horizontal pass
        for (int i = 1; i < 5; ++i)
        {
            result +=  texture(srcImage, uv + vec2(i * uvOffset.x, 0.f)).rgb * weights[i];
            result +=  texture(srcImage, uv - vec2(i * uvOffset.x, 0.f)).rgb * weights[i];
        }
    }
    else
    {
        // vertical pass
        for (int i = 1; i < 5; ++i)
        {
            result +=  texture(srcImage, uv + vec2(0.f, i * uvOffset.y)).rgb * weights[i];
            result +=  texture(srcImage, uv - vec2(0.f, i * uvOffset.y)).rgb * weights[i];
        }
    }
    fragColor = vec4(result, 1.f);
}