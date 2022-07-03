#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;
uniform sampler2D srcImage;

out vec4 fragcolor;

// reference: http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
vec3 sampleCustom13TapFilter(vec2 uv) {
    vec2 uvOffset = 1.f / textureSize(srcImage, 0);
    // 13 bilinear taps
    vec3 A = texture(srcImage, uv + vec2(-uvOffset.x,  uvOffset.y)).rgb; 
    vec3 B = texture(srcImage, uv + vec2( uvOffset.x,  uvOffset.y)).rgb; 
    vec3 C = texture(srcImage, uv + vec2(-uvOffset.x, -uvOffset.y)).rgb;
    vec3 D = texture(srcImage, uv + vec2( uvOffset.x, -uvOffset.y)).rgb;

    vec3 E = texture(srcImage, uv + vec2(-2.0 * uvOffset.x, 2.0 * uvOffset.y)).rgb;
    vec3 F = texture(srcImage, uv + vec2( 0.0             , 2.0 * uvOffset.y)).rgb;
    vec3 G = texture(srcImage, uv + vec2( 2.0 * uvOffset.x, 2.0 * uvOffset.y)).rgb;

    vec3 H = texture(srcImage, uv + vec2(-2.0 * uvOffset.x, 0.0)).rgb;
    vec3 I = texture(srcImage, uv + vec2( 0.0             , 0.0)).rgb;
    vec3 J = texture(srcImage, uv + vec2( 2.0 * uvOffset.x, 0.0)).rgb;

    vec3 K = texture(srcImage, uv + vec2(-2.0 * uvOffset.x, -2.0 * uvOffset.y)).rgb;
    vec3 L = texture(srcImage, uv + vec2( 0.0             , -2.0 * uvOffset.y)).rgb;
    vec3 M = texture(srcImage, uv + vec2( 2.0 * uvOffset.x, -2.0 * uvOffset.y)).rgb;

    // weighted average
    vec3 color = vec3(0.0);
    // E + F + H + I
    color += 0.125 * 0.25 * (E + F + H + I);
    // F + G + I + J
    color += 0.125 * 0.25 * (F + G + I + J);
    // H + I + K + L
    color += 0.125 * 0.25 * (H + I + K + L);
    // I + J + L + M
    color += 0.125 * 0.25 * (I + J + L + M);
    // A + B + C + D
    color += 0.5   * 0.25 * (A + B + C + D);
    return color;
}

void main()
{
    vec3 color = sampleCustom13TapFilter(psIn.texCoord0);
    fragcolor = vec4(color, 1.f);
}