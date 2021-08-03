#version 450 core

in vec2 uv;

out vec4 fragColor;

uniform sampler2D srcImage;
uniform sampler2D blendImage;

void main()
{
    vec2 uvOffset = 1.f / textureSize(srcImage, 0);
    vec2 boxUv_0 = uv + vec2(-uvOffset.x, uvOffset.y);
    vec2 boxUv_1 = uv + vec2(uvOffset.x, uvOffset.y);
    vec2 boxUv_2 = uv + vec2(-uvOffset.x, -uvOffset.y);
    vec2 boxUv_3 = uv + vec2(uvOffset.x, -uvOffset.y);

    vec3 upSampledColor = 0.25f * texture(srcImage, boxUv_0).rgb;
    upSampledColor += 0.25f * texture(srcImage,     boxUv_1).rgb;
    upSampledColor += 0.25f * texture(srcImage,     boxUv_2).rgb;
    upSampledColor += 0.25f * texture(srcImage,     boxUv_3).rgb;

    // vec3 upsampledColor = texture(srcImage, uv).rgb;
    vec3 blendColor = texture(blendImage, uv).rgb;
    fragColor = vec4(blendColor + upSampledColor, 1.f);
}