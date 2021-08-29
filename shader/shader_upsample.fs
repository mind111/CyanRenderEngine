#version 450 core

in vec2 uv;

out vec4 fragColor;

uniform sampler2D srcImage;
uniform sampler2D blendImage;

uniform int stageIndex;

// TODO: Use a better weights here
float weights[5] = {
    0.1, 0.2, 0.30, 0.4, 1.0
};

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

    float srcWeight = 1.0;
    // TODO: cleaup this hard-coded
    if (stageIndex == 0)
    {
        srcWeight = 0.0;
    }
    float weight = weights[stageIndex];
    vec3 blendColor = texture(blendImage, uv).rgb;
    fragColor = vec4(blendColor * weight + upSampledColor * srcWeight, 1.f);
}