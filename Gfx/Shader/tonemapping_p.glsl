#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec4 outColor;

float calcLuminance(in vec3 inLinearColor)
{   
    return 0.2126 * inLinearColor.r + 0.7152 * inLinearColor.g + 0.0722 * inLinearColor.b;
}

vec3 simpleReinhard(in vec3 inLinearColor) 
{
	return inLinearColor / (inLinearColor + 1.f);
}

vec3 luminanceReinhard(in vec3 inLinearColor) 
{
    float lum = calcLuminance(inLinearColor);
    float remappedLum = lum / (1.f + lum);
	return (inLinearColor / lum) * remappedLum;
}

/** note:
    taken from https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl 
*/
// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
const mat3 ACESInputMat =
{
    {0.59719, 0.35458, 0.04823},
    {0.07600, 0.90834, 0.01566},
    {0.02840, 0.13383, 0.83777}
};

// ODT_SAT => XYZ => D60_2_D65 => sRGB
const mat3 ACESOutputMat =
{
    { 1.60475, -0.53108, -0.07367},
    {-0.10208,  1.10813, -0.00605},
    {-0.00327, -0.07276,  1.07602}
};

vec3 RRTAndODTFit(vec3 v)
{
    vec3 a = v * (v + 0.0245786f) - 0.000090537f;
    vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

vec3 ACESFitted(vec3 color)
{
    color = transpose(ACESInputMat) * color;
    // Apply RRT and ODT
    color = RRTAndODTFit(color);
    color = transpose(ACESOutputMat) * color;
    // Clamp to [0, 1]
    color = clamp(color, vec3(0.), vec3(1.));
    return color;
}

vec3 acesFilm(vec3 x) 
{
    //Aces film curve
    return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.,1.);
}

vec3 gammaCorrection(in vec3 inLinearColor, float gamma)
{
	return vec3(pow(inLinearColor.r, gamma), pow(inLinearColor.g, gamma), pow(inLinearColor.b, gamma));
}

vec3 tonemapping(vec3 linearColor, float exposure)
{
    vec3 tonemapped;
#if 1
    // tonemapped = luminanceReinhard(linearColor * exposure);
    // tonemapped = gammaCorrection(tonemapped, 1.f / 2.2f);
    tonemapped = acesFilm(linearColor * exposure);
    tonemapped = gammaCorrection(tonemapped, 1.f / 2.2f);
#else
    tonemapped = ACESFitted(gammaCorrection(exposure * linearColor, 1.f / 2.2f));
#endif
	return tonemapped;
}

uniform sampler2D srcTexture;

void main()
{
    vec3 linearColor = texture(srcTexture, psIn.texCoord0).rgb;
    vec3 tonemapped = tonemapping(linearColor, 1.f);
    outColor = vec4(tonemapped, 1.f);
}
