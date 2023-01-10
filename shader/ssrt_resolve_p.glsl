#version 450 core
#define PI 3.1415926

in VSOutput
{
	vec2 texCoord0;
} psIn;

layout(location = 2) out vec3 outIrradiance;

uniform sampler2D depthBuffer;
uniform sampler2D normalBuffer;
uniform sampler2DArray hitPositionBuffer;
uniform sampler2DArray hitNormalBuffer;
uniform sampler2DArray hitRadianceBuffer;
uniform sampler2D blueNoiseTexture;

layout(std430) buffer ViewBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
};

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) 
{
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
}

vec3 worldToScreen(vec3 pp, in mat4 view, in mat4 projection)
{
    vec4 p = projection * view * vec4(pp, 1.f);
    p /= p.w;
    return p.xyz * .5f + .5f;
}

/**
* blue noise samples on a unit disk taken from https://www.shadertoy.com/view/3sfBWs
*/
const vec2 BlueNoiseInDisk[64] = vec2[64](
    vec2(0.478712,0.875764),
    vec2(-0.337956,-0.793959),
    vec2(-0.955259,-0.028164),
    vec2(0.864527,0.325689),
    vec2(0.209342,-0.395657),
    vec2(-0.106779,0.672585),
    vec2(0.156213,0.235113),
    vec2(-0.413644,-0.082856),
    vec2(-0.415667,0.323909),
    vec2(0.141896,-0.939980),
    vec2(0.954932,-0.182516),
    vec2(-0.766184,0.410799),
    vec2(-0.434912,-0.458845),
    vec2(0.415242,-0.078724),
    vec2(0.728335,-0.491777),
    vec2(-0.058086,-0.066401),
    vec2(0.202990,0.686837),
    vec2(-0.808362,-0.556402),
    vec2(0.507386,-0.640839),
    vec2(-0.723494,-0.229240),
    vec2(0.489740,0.317826),
    vec2(-0.622663,0.765301),
    vec2(-0.010640,0.929347),
    vec2(0.663146,0.647618),
    vec2(-0.096674,-0.413835),
    vec2(0.525945,-0.321063),
    vec2(-0.122533,0.366019),
    vec2(0.195235,-0.687983),
    vec2(-0.563203,0.098748),
    vec2(0.418563,0.561335),
    vec2(-0.378595,0.800367),
    vec2(0.826922,0.001024),
    vec2(-0.085372,-0.766651),
    vec2(-0.921920,0.183673),
    vec2(-0.590008,-0.721799),
    vec2(0.167751,-0.164393),
    vec2(0.032961,-0.562530),
    vec2(0.632900,-0.107059),
    vec2(-0.464080,0.569669),
    vec2(-0.173676,-0.958758),
    vec2(-0.242648,-0.234303),
    vec2(-0.275362,0.157163),
    vec2(0.382295,-0.795131),
    vec2(0.562955,0.115562),
    vec2(0.190586,0.470121),
    vec2(0.770764,-0.297576),
    vec2(0.237281,0.931050),
    vec2(-0.666642,-0.455871),
    vec2(-0.905649,-0.298379),
    vec2(0.339520,0.157829),
    vec2(0.701438,-0.704100),
    vec2(-0.062758,0.160346),
    vec2(-0.220674,0.957141),
    vec2(0.642692,0.432706),
    vec2(-0.773390,-0.015272),
    vec2(-0.671467,0.246880),
    vec2(0.158051,0.062859),
    vec2(0.806009,0.527232),
    vec2(-0.057620,-0.247071),
    vec2(0.333436,-0.516710),
    vec2(-0.550658,-0.315773),
    vec2(-0.652078,0.589846),
    vec2(0.008818,0.530556),
    vec2(-0.210004,0.519896) 
);

uniform float reuseKernelRadius;
uniform int numReuseSamples;

void main()
{
	vec3 textureDim = textureSize(hitPositionBuffer, 0);
	int numSamples = int(textureDim.z);
	vec3 irradiance = vec3(0.f);

	float depth = texture(depthBuffer, psIn.texCoord0).r;

	if (depth > 0.9999f)
	{
		discard;
	}
	vec3 worldSpaceRO = screenToWorld(vec3(psIn.texCoord0, depth) * 2.f - 1.f, inverse(view), inverse(projection));
	vec3 worldSpaceNormal = normalize(texture(normalBuffer, psIn.texCoord0).rgb * 2.f - 1.f);

    int totalNumSamples = 0;
	// no reuse first
	for (int i = 0; i < numSamples; ++i)
	{
		vec3 texCoord = vec3(psIn.texCoord0, i);

		vec4 hitPosition = texture(hitPositionBuffer, texCoord).rgba;
		if (hitPosition.w > 0.f)
		{
			vec3 hitNormal = texture(hitNormalBuffer, texCoord).rgb;
			vec3 hitRadiance = texture(hitRadianceBuffer, texCoord).rgb;
			vec3 rd = normalize(hitPosition.xyz - worldSpaceRO);
			bool bInUpperHemisphere = dot(hitNormal, -rd) > 0.f;
			if (bInUpperHemisphere)
			{
				irradiance += hitRadiance * max(dot(worldSpaceNormal, rd), 0.f);
			}
		}
        totalNumSamples += 1;

        // reuse
        for (int s = 0; s < numReuseSamples; ++s)
        {
            /** note - @min: this random rotation here is not friendly to the texture cache
            * are there any remedies to this ...?
            */
			float randomRotation = texture(blueNoiseTexture, gl_FragCoord.xy / float(textureSize(blueNoiseTexture, 0).xy)).r * PI * 2.f;
			mat2 rotation = {
				{ cos(randomRotation), sin(randomRotation) },
				{ -sin(randomRotation), cos(randomRotation) }
			};
            vec3 sampleCoord = vec3(texCoord.xy + rotation * (BlueNoiseInDisk[s] * reuseKernelRadius), i);

            // reject neighbor samples if the depth/normal difference is too big
            float neighborDepth = texture(depthBuffer, sampleCoord.xy).r;
            vec3 neighborNormal = normalize(texture(normalBuffer, sampleCoord.xy).rgb * 2.f - 1.f);
            if (abs(neighborDepth - depth) > 0.01 && dot(neighborNormal, worldSpaceNormal) < 0.8)
            {
				continue;
            }

            // todo: deal with the edge case where the kernel taps goes outside of the texture space

			vec4 hitPosition = texture(hitPositionBuffer, sampleCoord).rgba;
			if (hitPosition.w > 0.f)
			{
				vec3 hitNormal = texture(hitNormalBuffer, sampleCoord).rgb;
				vec3 hitRadiance = texture(hitRadianceBuffer, sampleCoord).rgb;
				vec3 rd = normalize(hitPosition.xyz - worldSpaceRO);
				bool bInUpperHemisphere = dot(hitNormal, -rd) > 0.f;
				irradiance += bInUpperHemisphere ? hitRadiance * max(dot(worldSpaceNormal, rd), 0.f) : vec3(0.f);
                /** note - @min:
                * Only count neighbor rays that found a intersection, not sure if this is correct way of doing this,
                * but simply counting every neighbor tap as valid will makes the output "darker" as not every neighbor
                * taps found a hit. This helps preserve the overall brightness of the resulting indirect irradiance
                * but
                */
                totalNumSamples += 1;
			}
        }
	}

	// outIrradiance = irradiance / float(numSamples + numSamples * numReuseSamples);
	outIrradiance = irradiance / float(totalNumSamples);
}
