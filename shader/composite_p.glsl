#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec4 outColor;

uniform float exposure;
uniform float colorTempreture;
uniform float enableBloom;
uniform float bloomIntensity;
uniform float enableTonemapping;
uniform uint tonemapOperator;
uniform float whitePointLuminance;

uniform sampler2D sceneColorTexture;
uniform sampler2D bloomColorTexture;

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

float calcLuminance(in vec3 inLinearColor)
{   
    return 0.2126 * inLinearColor.r + 0.7152 * inLinearColor.g + 0.0722 * inLinearColor.b;
}

vec3 adjustExposure(vec3 inLinearColor)
{
    return vec3(0.f);
}

/** note - @mind:
* run into an issue where input inLinearColor is (inf, inf, inf), and then the luminance becomes inf
* leading the division by luminance becoming 0. Should luminance be bound by whitePointLuminance...?
*/
#define MAX_RGB 1000.f
vec3 ReinhardTonemapping(in vec3 inLinearColor, float whitePointLum)
{
    vec3 color = vec3(min(MAX_RGB, inLinearColor.r), min(MAX_RGB, inLinearColor.g), min(MAX_RGB, inLinearColor.b));
    float luminance = calcLuminance(color);
    color = inLinearColor / luminance;
    float numerator = luminance * (1. + luminance / (whitePointLum * whitePointLum));
    float remappedLuminance = numerator / (1.f + luminance);
    return color * remappedLuminance;
}

vec3 gammaCorrection(in vec3 inLinearColor, float gamma)
{
	return vec3(pow(inLinearColor.r, gamma), pow(inLinearColor.g, gamma), pow(inLinearColor.b, gamma));
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

/**
* linear tonemap operator
*/
vec3 linearTonemap()
{
    vec3 tonemapped = vec3(0.f);
    return tonemapped;
}

/** 
	* Taken from https://www.shadertoy.com/view/4sc3D7
*/
vec3 colorTemperatureToRGB(const in float temperature){
  // Values from: http://blenderartists.org/forum/showthread.php?270332-OSL-Goodness&p=2268693&viewfull=1#post2268693   
  mat3 m = (temperature <= 6500.0) ? mat3(vec3(0.0, -2902.1955373783176, -8257.7997278925690),
	                                      vec3(0.0, 1669.5803561666639, 2575.2827530017594),
	                                      vec3(1.0, 1.3302673723350029, 1.8993753891711275)) : 
	 								 mat3(vec3(1745.0425298314172, 1216.6168361476490, -8257.7997278925690),
   	                                      vec3(-2666.3474220535695, -2173.1012343082230, 2575.2827530017594),
	                                      vec3(0.55995389139931482, 0.70381203140554553, 1.8993753891711275)); 
  return mix(clamp(vec3(m[0] / (vec3(clamp(temperature, 1000.0, 40000.0)) + m[1]) + m[2]), vec3(0.0), vec3(1.0)), vec3(1.0), smoothstep(1000.0, 0.0, temperature));
}

void main()
{
    const float bloomIntensity = .7f;
    vec3 linearColor = texture(sceneColorTexture, psIn.texCoord0).rgb;

    // adjust color tempreture
    // linearColor *= colorTemperatureToRGB(colorTempreture);

    if (enableBloom > 0.5f)
    {
        // linear blending
       // linearColor = mix(linearColor, texture(bloomColorTexture, psIn.texCoord0).rgb, .5);
       // additive blending
       linearColor = linearColor + texture(bloomColorTexture, psIn.texCoord0).rgb;
    }

#define TONEMAPPER_REINHARD 0
#define TONEMAPPER_ACES 1

    // tone mapping
    if (enableTonemapping > .5f)
    {
        vec3 tonemappedColor = vec3(0.f);
        if (tonemapOperator == TONEMAPPER_ACES)
        {
			tonemappedColor = ACESFitted(gammaCorrection(exposure * linearColor, 1.f / 2.2f));
		}
        else if (tonemapOperator == TONEMAPPER_REINHARD)
        {
			tonemappedColor = ReinhardTonemapping(exposure * linearColor, whitePointLuminance);
			tonemappedColor = gammaCorrection(tonemappedColor, 1.f / 2.2f);
        }
		outColor = vec4(tonemappedColor, 1.f);
	}
    else
    {
        outColor = vec4(gammaCorrection(linearColor, 1.f / 2.2f), 1.f);
    }
}