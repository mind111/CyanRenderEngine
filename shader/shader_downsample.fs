#version 450 core

in vec2 uv;

out vec4 fragcolor;

uniform sampler2D srcImage;

float hash(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898,78.233)))*43758.5453123);
}

void main()
{
    vec2 uvOffset = 1.f / textureSize(srcImage, 0);
    // TODO: add a random jitter to the filter center
    vec2 jitter = vec2(hash(uv.xy), hash(uv.yx));
    vec2 jitteredUv = uv + vec2(uvOffset.x * jitter.x, uvOffset.y * jitter.y);
    // box filter
    vec2 boxUv_0 = uv + vec2(-uvOffset.x, uvOffset.y);
    vec2 boxUv_1 = uv + vec2(uvOffset.x, uvOffset.y);
    vec2 boxUv_2 = uv + vec2(-uvOffset.x, -uvOffset.y);
    vec2 boxUv_3 = uv + vec2(uvOffset.x, -uvOffset.y);
    vec3 color = texture(srcImage, boxUv_0).rgb;
    color += texture(srcImage,     boxUv_1).rgb;
    color += texture(srcImage,     boxUv_2).rgb;
    color += texture(srcImage,     boxUv_3).rgb;

    vec2 boxUv_00 = boxUv_0 + vec2(-uvOffset.x, uvOffset.y);
    vec2 boxUv_01 = boxUv_1 + vec2(uvOffset.x, uvOffset.y);
    vec2 boxUv_02 = boxUv_2 + vec2(-uvOffset.x, -uvOffset.y);
    vec2 boxUv_03 = boxUv_3 + vec2(uvOffset.x, -uvOffset.y);
    color += texture(srcImage,     boxUv_00).rgb;
    color += texture(srcImage,     boxUv_01).rgb;
    color += texture(srcImage,     boxUv_02).rgb;
    color += texture(srcImage,     boxUv_03).rgb;
    color *= 0.125f;
    fragcolor = vec4(color, 1.f);
}