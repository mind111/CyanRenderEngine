#version 450 core

in vec3 n;
in vec3 wn;
in vec3 t;
in vec2 uv;
in vec2 uv1;
in vec3 fragmentPos;
in vec3 fragmentPosWS;
in vec4 shadowPos;

layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec3 fragmentNormal; 
layout (location = 2) out vec3 fragmentDepth; 
layout (location = 3) out vec3 radialDistance;
layout (location = 4) out float shadowMask;

#define SlopeBasedBias 0

// out vec4 fragColor;

//- transforms
uniform mat4 s_view;
//- misc
uniform float kDiffuse;
uniform float kSpecular;
uniform float directDiffuseSlider;
uniform float directSpecularSlider;
uniform float indirectDiffuseSlider;
uniform float indirectSpecularSlider;
uniform float directLightingSlider;
uniform float indirectLightingSlider;

uniform struct MaterialProperty
{
    float hasDiffuseMap;
    float usePrototypeTexture;
    float hasAoMap;
    float hasNormalMap;
    float hasRoughnessMap;
    float hasMetelnessMap;
    float hasMetallicRoughnessMap;
    float hasBakedLighting;
} uMaterialProps; 

uniform float uniformSpecular; // control incident specular amount .5 by default
uniform float uniformRoughness;
uniform float uniformMetallic;
uniform vec4 flatColor;
uniform int numPointLights; //non-material
uniform int numDirLights;   //non_material

//- samplers
uniform sampler2D diffuseMaps[2];
uniform sampler2D emissionMaps[2];
uniform sampler2D normalMap;
uniform sampler2D roughnessMap;
uniform sampler2D metalnessMap;
uniform sampler2D metallicRoughnessMap;
uniform sampler2D aoMap;
uniform samplerCube envmap;             //non-material
uniform samplerCube irradianceDiffuse;  //non-material
uniform samplerCube irradianceSpecular; //non-material
uniform sampler2D   brdfIntegral;
uniform sampler2D   lightMap;
uniform sampler2D   ssaoTex;

//- debug switches
uniform float debugNormalMap;
uniform float debugAO;
uniform float debugD;
uniform float debugF;
uniform float debugG;
uniform float disneyReparam;
// experiemental
uniform float wrap;

// shaodw
struct ShadowCascade
{
    sampler2D depthMap;
    mat4 lightProjection;
};
uniform ShadowCascade shadowCascades[4];

uniform mat4 lightView;

#define pi 3.14159265359
float cascadeIntervals[4] = {0.1f, 0.3f, 0.6f, 1.0f};

struct Light 
{
    vec4 color;
};

struct DirLight
{
    // float padding[2]; // 8 bytes of padding
    vec4 color;
    vec4 direction;
};

struct PointLight
{
    // float padding[2]; // 8 bytes of padding
    vec4 color;
    vec4 position;
};

// TODO: shader storage buffer is slower compared to uniform buffer, maybe switch to use ubo instead?
layout(std430, binding = 1) buffer dirLightsData
{
    DirLight lights[];
} dirLightsBuffer;

layout(std430, binding = 2) buffer pointLightsData
{
    PointLight lights[];
} pointLightsBuffer;

// precomputed GI
uniform struct Lighting
{
    samplerCube irradianceProbe;
    samplerCube localReflectionProbe;
} gLighting;

// TODO: Handedness ...?
vec4 tangentSpaceToViewSpace(vec3 tn, vec3 vn, vec3 t) 
{
    mat4 tbn;
    // apply Gram-Schmidt process
    t = normalize(t - dot(t, vn) * vn);
    vec3 b = cross(vn, t);
    tbn[0] = vec4(t, 0.f);
    tbn[1] = vec4(b, 0.f);
    tbn[2] = vec4(vn, 0.f);
    tbn[3] = vec4(0.f, 0.f, 0.f, 1.f);
    return tbn * vec4(tn, 0.0f);
}

// Generate a sample direction in tangent space and output world space direction
vec3 generateSample(vec3 n, float theta, float phi)
{
    float x = sin(theta) * sin(phi);
    float y = sin(theta) * cos(phi);
    float z = cos(theta);
    vec3 s = vec3(x, y, z);
    // Prevent the case where n is  (0.f, 1.f, 0.f)
    vec3 up = abs(n.x) > 0.f ? vec3(0.f, 1.f, 0.f) : vec3(0.f, 0.f, 1.f);
    vec3 xAxis = cross(up, n);
    vec3 yAxis = cross(n, xAxis);
    vec3 zAxis = n;
    mat3 toWorldSpace = {
        xAxis,
        yAxis,
        zAxis
    };
    return normalize(toWorldSpace * s);
}

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

float VanDerCorput(uint n, uint base)
{
    float invBase = 1.0 / float(base);
    float denom   = 1.0;
    float result  = 0.0;
    for(uint i = 0u; i < 32u; ++i)
    {
        if(n > 0u)
        {
            denom   = mod(float(n), 2.0);
            result += denom * invBase;
            invBase = invBase / 2.0;
            n       = uint(float(n) / 2.0);
        }
    }
    return result;
}

vec2 HammersleyNoBitOps(uint i, uint N)
{
    return vec2(float(i)/float(N), VanDerCorput(i, 2u));
}

// TODO: Rename this or refactor this to handle gamma for both directions
vec3 gammaCorrection(vec3 inColor)
{
    return vec3(pow(inColor.r, 2.2f), pow(inColor.g, 2.2f), pow(inColor.b, 2.2f));
}

// Taking the formula from the GGX paper and simplify to avoid computing tangent
// Using Disney's parameterization of alpha_g = roughness * roughness
float GGX(float roughness, float ndoth) 
{
    float alpha = roughness;
    if (disneyReparam > .5f)
        alpha *= alpha;
    float alpha2 = alpha * alpha;
    float result = alpha2;
    float denom = ndoth * ndoth * (alpha2 - 1.f) + 1.f;
    // prevent divide by infinity at the same time will produce 
    // incorrect specular highlight when roughness is really low.
    // when roughness is really low, specular highlights are supposed to be
    // really tiny and noisy.
    result /= max((pi * denom * denom), 0.0001f); 
    return result;
}

// TODO: implement fresnel that takes roughness into consideration
vec3 fresnel(vec3 f0, vec3 n, vec3 v)
{
    float ndotv = saturate(dot(n, v));
    float fresnelCoef = 1.f - ndotv;
    fresnelCoef = fresnelCoef * fresnelCoef * fresnelCoef * fresnelCoef * fresnelCoef;
    // f0: fresnel reflectance at incidence angle 0
    // f90: fresnel reflectance at incidence angle 90, f90 in this case is vec3(1.f) 
    return mix(f0, vec3(1.f), fresnelCoef);
}

// when roughness is 0, this term is 0
/*
    lambda function in Smith geometry term
*/
float ggxSmithLambda(vec3 v, vec3 h, float roughness)
{
    float alpha = roughness;
    if (disneyReparam > .5f)
        alpha *= roughness;
    float alpha2 = alpha * alpha;
    float hdotv = saturate(dot(h, v));
    float hdotv2 = max(0.001f, hdotv * hdotv);
    // float a2 = hdotv2 / (alpha2 * (1.f - hdotv * hdotv));
    float a2Rcp = alpha2 * (1.f - hdotv2) / hdotv2;
    return 0.5f * (sqrt(1.f + a2Rcp) - 1.f);
}

/*
    * height-correlated Smith geometry attenuation 
*/
float ggxSmithG2(vec3 v, vec3 l, vec3 h, float roughness)
{
    float ggxV = ggxSmithLambda(v, h, roughness);
    float ggxL = ggxSmithLambda(l, h, roughness);
    return 1.f / (1.f + ggxV + ggxL);
}

/* 
    non-height correlated smith G2
*/
float ggxSmithG1(vec3 v, vec3 h, vec3 n, float roughness)
{
    float ndotv = dot(n, v); 
    float hdotv = dot(h, v); 
    float alpha = roughness;
    if (disneyReparam > .5f)
        alpha *= roughness;
    float alpha2 = alpha * alpha;
    float tangentTheta2 = (1 - ndotv * ndotv) / max(0.001f, (ndotv * ndotv));
    return 2.f / (1.f + sqrt(1.f + alpha2 * tangentTheta2));
}

float ggxSmithG2Ex(vec3 v, vec3 l, vec3 n, float roughness)
{
    vec3 h = normalize(v + n);
    return ggxSmithG1(v, h, n, roughness) * ggxSmithG1(l, h, n, roughness);
}

/*
    * microfacet diffuse brdf
*/
vec3 diffuseBrdf(vec3 baseColor)
{
    return baseColor / pi;
}

// TODO: debug this, the specular highlight is too high
/*
    * microfacet specular brdf 
    * A brdf fr(i, o, n) 
*/
vec3 specularBrdf(vec3 wi, vec3 wo, vec3 n, float roughness, vec3 f0)
{
    float ndotv = saturate(dot(n, wo));
    float ndotl = saturate(dot(n, wi));
    vec3 h = normalize(wi + wo);
    float ndoth = saturate(dot(n, h));
    float D = GGX(roughness, ndoth);
    float G = ggxSmithG2(wo, wi, n, roughness);
    // float G = ggxSmithG2Ex(wo, wi, n, roughness);
    vec3 F = fresnel(f0, n, wo);
    // TODO: the max() operator here actually affects how strong the specular highlight is
    // around grazing angle. Using a small value like 0.0001 will have pretty bad shading alias. However,
    // is using 0.1 here corret though. Need to reference other implementation. 
    vec3 brdf = D * F * G / max((4.f * ndotv * ndotl), 0.01f);
    return brdf;
}

struct RenderParams
{
    vec3 baseColor;
    vec3 f0;
    float roughness;
    float metallic;
    vec3 n;
    vec3 wn;
    vec3 v;
    vec3 l;
    vec3 li;
    float ao;
    float shadow;
};

/*
    * render equation:
    *    fr(i, o, n) * Li * cos(i, n)
*/
vec3 render(RenderParams params)
{
    vec3 h = normalize(params.v + params.l);
    float ndotl = max(0.000f, dot(params.n, params.l));
    vec3 F = fresnel(params.f0, params.n, params.v);
    vec3 kDiffuse = mix(vec3(1.f) - F, vec3(0.0f), params.metallic);
    vec3 diffuse = kDiffuse * diffuseBrdf(params.baseColor) * ndotl;
    vec3 specular = specularBrdf(params.l, params.v, params.n, params.roughness, params.f0) * ndotl * uniformSpecular;
    return (directDiffuseSlider * diffuse * params.ao + directSpecularSlider * specular) * params.li * params.shadow;
}

struct CascadeOffset
{
    int cascadeIndex;
    float blend;
};

CascadeOffset computeCascadeIndex()
{
    // determine which cascade to sample
    float t = (abs(fragmentPos.z) - 0.5f) / (100.f - 0.5f);
    int cascadeIndex = 0;
    float blend = 1.f;
    for (int i = 0; i < 4; ++i)
    {
        if (t < cascadeIntervals[i])
        {
            cascadeIndex = i;
            if (i > 0)
            {
                float cascadeRelDepth = (t - cascadeIntervals[i-1]) / (cascadeIntervals[i] - cascadeIntervals[i-1]); 
                cascadeRelDepth = clamp(cascadeRelDepth, 0.f, 0.2f);
                blend = cascadeRelDepth * 5.f;
            }
            break;
        }
    }
    return CascadeOffset(cascadeIndex, blend);
}

float receiverPlaneBias()
{
    return 0.f;
}

// TODO: improve shadow biasing, normal bias and receiver plane bias...?
// TODO: add a manual depth bias for each dir light maybe...?
// TODO: proper blending between cascades
float PCFShadow(CascadeOffset cascadeOffset, float bias)
{
    float shadow = 1.f;

    // compute shadowmap uv
    vec4 lightViewFragPos = shadowCascades[cascadeOffset.cascadeIndex].lightProjection * lightView * vec4(fragmentPosWS, 1.f); 
    vec2 shadowTexCoord = lightViewFragPos.xy * .5f + .5f; 

    // bring depth into [0, 1] from [-1, 1]
    float fragmentDepth = lightViewFragPos.z * 0.5f + .5f;

    // clip uv used to sample the shadow map 
    vec2 texelOffset = vec2(1.f) / textureSize(shadowCascades[cascadeOffset.cascadeIndex].depthMap, 0);

    // filtering when sampling
    vec2 samples[9] = { 
        vec2(-1.f, 1.f), vec2(0.f, 1.f), vec2(1.f, 1.f),
        vec2(-1.f, 0.f), vec2(0.f, 0.f), vec2(1.f, 0.f),  
        vec2(-1.f, -1.f),vec2(0.f, -1.f), vec2(1.f, -1.f)
    };

    const int kernelRaius = 4;
    // 5 x 5 filter kernel
    // float kernel[25] = {
    //     0.04, 0.04, 0.04, 0.04, 0.04,
    //     0.04, 0.04, 0.04, 0.04, 0.04,
    //     0.04, 0.04, 0.04, 0.04, 0.04,
    //     0.04, 0.04, 0.04, 0.04, 0.04,
    //     0.04, 0.04, 0.04, 0.04, 0.04
    // };

    // 7 x 7 filter kernel
    float kernel[49] = {
        0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02,
        0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02,
        0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02,
        0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02,
        0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02,
        0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02,
        0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02,
    };
    float convolved = 0.f;
    for (int i = -kernelRaius; i <= kernelRaius; ++i)
    {
        for (int j = -kernelRaius; j <= kernelRaius; ++j)
        {
            vec2 offset = vec2(i, j) * texelOffset;
            vec2 sampleTexCoord = shadowTexCoord + offset;
            if (sampleTexCoord.x < 0.f 
                || sampleTexCoord.x > 1.f 
                || sampleTexCoord.y < 0.f 
                || sampleTexCoord.y > 1.f) 
                continue;

            float depthTest = texture(shadowCascades[cascadeOffset.cascadeIndex].depthMap, sampleTexCoord).r < (fragmentDepth - bias) ? 0.f : 1.f;
            convolved += depthTest * kernel[(i + kernelRaius) * kernelRaius + (j + kernelRaius)];
        }
    }
    shadow = convolved;
    return shadow;
}

// TODO: the shadow is almost too soft!
/* 
    Note(Min): objects that are closer to the near clipping plane for each frusta in CSM doesn't show such 
    artifact, which infers that when t decreases, the Chebychev inequility increases. Don't know why this happens!
*/
float varianceShadow(vec2 shadowTexCoord, float fragmentDepth, int cascadeOffset)
{
    float shadow = 1.f;
    float firstMoment = texture(shadowCascades[cascadeOffset].depthMap, shadowTexCoord).r;
    if (firstMoment <= fragmentDepth)
    {
        float t = fragmentDepth;
        float secondMoment = texture(shadowCascades[cascadeOffset].depthMap, shadowTexCoord).g;
        // use second and first moment to compute variance
        float variance = max(secondMoment - firstMoment * firstMoment, 0.0001f);
        //he Chebycv visibility test
        shadow = variance / (variance + (t - firstMoment) * (t - firstMoment));
    }
    return shadow;
}

#define PCF_SHADOW   1
#define VSM_SHADOW   0

float slopeBasedBias(vec3 n, vec3 l)
{
    float cosAlpha = max(dot(n, l), 0.f);
    float tanAlpha = tan(acos(cosAlpha));
    float bias = clamp(tanAlpha * 0.001f, 0.f, 1.f);
    return bias;
}

float constantBias()
{
    return 0.001f;
}

#define BLEND_CASCADES 0
float isInShadow(float bias)
{
    float shadow = 1.f;

    // determine which cascade to sample
    CascadeOffset cascadeOffset = computeCascadeIndex();

#if PCF_SHADOW
#if BLEND_CASCADES
    if (cascadeOffset.blend < 1.f)
    {
        float shadow0 = PCFShadow(cascadeOffset, bias);
        cascadeOffset.cascadeIndex -= 1;
        float shadow1 = PCFShadow(cascadeOffset, bias);
        shadow = mix(shadow0, shadow1, cascadeOffset.blend);
    }
    else
    {
        shadow = PCFShadow(cascadeOffset, bias);
    }
#else
        shadow = PCFShadow(cascadeOffset, bias);
#endif
#endif
#if VSM_SHADOW
    shadow = varianceShadow(shadowTexCoord, fragmentDepth, cascadeOffset);
#endif
    shadowMask = shadow;
    return shadow;
}

vec3 directLighting(RenderParams renderParams)
{
    vec3 color = vec3(0.f);
    // TODO: area lights?
    {
    }
    for (int i = 0; i < numPointLights; i++)
    {
        vec4 lightPos = s_view * pointLightsBuffer.lights[i].position;
        float distance = length(lightPos.xyz - fragmentPos);
        // float attenuation = distance > 5.f ? 0.f : 1.f / (1.0f + 0.7 * distance + 1.8 * distance * distance);
        float attenuation = 1.f / (1.0f + 0.7 * distance + 1.8 * distance * distance);

        // light dir
        renderParams.l = normalize(lightPos.xyz - fragmentPos);
        // light color
        renderParams.li = pointLightsBuffer.lights[i].color.rgb * pointLightsBuffer.lights[i].color.w * attenuation;
        renderParams.shadow = 1.f;
        color += render(renderParams);
    }
    for (int i = 0; i < numDirLights; i++)
    {
        // sample shadow map
        vec4 worldFragPos = inverse(s_view) * vec4(fragmentPos, 1.f);
        vec4 lightDir = s_view * (dirLightsBuffer.lights[i].direction);
        // light dir
        renderParams.l = normalize(lightDir.xyz);
#if SlopeBasedBias
        float shadowBias = slopeBasedBias(renderParams.n, renderParams.l);
#else
        float shadowBias = constantBias();
#endif
        renderParams.shadow = isInShadow(shadowBias);
        // light color
        renderParams.li = dirLightsBuffer.lights[i].color.rgb * dirLightsBuffer.lights[i].color.w;
        color += render(renderParams);
    }
    return color;
}

// ground-truth specular IBL
vec3 specularIBL(vec3 f0, vec3 normal, vec3 viewDir, float roughness)
{
    float numSamples = 512.f;
    vec3 specularE = vec3(0.f);
    for (uint sa = 0; sa < numSamples; sa++)
    {
        vec2 rand_uv = HammersleyNoBitOps(sa, uint(numSamples));
        // Random samples a microfacet normal following GGX distribution
        // float rand_u = hash(sa * 12.3f / numSamples);
        // float rand_v = hash(sa * 78.2f / numSamples); 
        float rand_u = rand_uv.x; 
        float rand_v = rand_uv.y; 

        // TODO: verify this importance sampling ggx procedure
        float theta = atan(roughness * sqrt(rand_u) / sqrt(1 - rand_v));
        float phi = 2 * pi * rand_v;
        vec3 h = generateSample(normal, theta, phi);

        // Reflect viewDir against microfacet normal to get incident direction
        vec3 vi = -reflect(viewDir, h);
        // Sample a color from envmap
        vec3 envColor = texture(envmap, vi).rgb;
        // Shading
        float ndotv = saturate(dot(normal, viewDir));
        float ndoth = saturate(dot(normal, h));
        float vdoth = saturate(dot(viewDir, h)); 
        float ndotl = saturate(dot(normal, vi));
        // cook-torrance microfacet shading model
        float shadowing = ggxSmithG2(vi, viewDir, h, roughness);
        vec3 fresnel = fresnel(f0, vi, h);
        /* 
            Notes:
                * Compared to the BRDF equation used in radiance(), 4 * ndotl * ndotv is missing,
                because they got cancelled out by the pdf.

                * pdf = (D * ndoth) / (4 * vdoth) 

                * Notice that the GGX distribution term D is also cancelled out here from the original 
                    microfacet BRDF fr = DFG / (4 * ndotv * ndotl)
                    when doing importance sampling.
        */
        vec3 nom = fresnel * shadowing * vdoth;
        float denom = max(0.0001f, ndotv * ndoth);
        specularE += envColor * (nom / denom);
    }
    specularE /= numSamples;
    return specularE;
}

vec3 indirectLighting(RenderParams params)
{
    vec3 color = vec3(0.f);
    vec3 F = fresnel(params.f0, params.n, params.v); 
    vec3 kDiffuse = mix(vec3(1.f) - F, vec3(0.f), params.metallic);
    // diffuse irradiance
    vec3 diffuse = 0.f * kDiffuse * params.baseColor * texture(irradianceDiffuse, params.wn).rgb; 
    // specular radiance
    float ndotv = saturate(dot(params.n, params.v));
    vec3 r = -reflect(params.v, params.n);
    mat4 viewRotation = s_view;
    // Cancel out the translation part of the view matrix so that the cubemap will always follow
    // the camera, view of the cubemap will change along with the change of camera rotation 
    viewRotation[3][0] = 0.f;
    viewRotation[3][1] = 0.f;
    viewRotation[3][2] = 0.f;
    viewRotation[3][3] = 1.f;
    vec3 rr = (inverse(viewRotation) * vec4(r, 0.f)).xyz;
    vec3 prefilteredColor = textureLod(irradianceSpecular, rr, params.roughness * 10.f).rgb;
    vec3 brdf = texture(brdfIntegral, vec2(params.roughness, ndotv)).rgb; 
    vec3 specular = (prefilteredColor * uniformSpecular) * (params.f0 * brdf.r + brdf.g);

    // probe based diffuse GI
    {
        diffuse += kDiffuse * params.baseColor * texture(gLighting.irradianceProbe, params.wn).rgb;
    }

    color += indirectDiffuseSlider * diffuse + indirectSpecularSlider * specular;
    return color;
}

float hash(float seed)
{
    return fract(sin(seed)*43758.5453);
}

vec3 prototypeGridTexture(vec3 worldPos)
{
    float coef = (abs(worldPos.x - round(worldPos.x)) < 0.01f) || (abs(worldPos.z - round(worldPos.z)) < 0.01f) ? .2f : 1.f;
    vec3 color = flatColor.rgb * coef;
    return color;
}

vec3 defaultAlbedo(vec3 worldPos, vec2 texCoord)
{
    vec3 color = flatColor.rgb;
    color = uMaterialProps.usePrototypeTexture > .5f ? prototypeGridTexture(worldPos) : color;
    return color;
}

void main() 
{
    /* Normal mapping */
    // Interpolation done by the rasterizer may change the length of the normal 
    vec3 normal = normalize(n);
    vec3 tangent = normalize(t);
    vec3 worldSpaceNormal = normalize(wn);

    if (uMaterialProps.hasNormalMap > 0.5f)
    {
        vec3 tn = texture(normalMap, uv).xyz;
        // Convert from [0, 1] to [-1.0, 1.0] and renomalize if texture filtering changes the length
        tn = normalize(tn * 2.f - vec3(1.f)); 
        // Covert normal from tangent frame to camera space
        normal = tangentSpaceToViewSpace(tn, normal, tangent).xyz;
    }

    /* Texture mapping */
    vec4 albedo = uMaterialProps.hasDiffuseMap > .5f ? texture(diffuseMaps[0], uv) : vec4(defaultAlbedo(fragmentPosWS, uv), 1.f);
    // from sRGB to linear space
    albedo.rgb = vec3(pow(albedo.r, 2.2f), pow(albedo.g, 2.2f), pow(albedo.b, 2.2f));

    // According to gltf-2.0 spec, metal is sampled from b, roughness is sampled from g
    float roughness, metallic;
    if (uMaterialProps.hasMetallicRoughnessMap > 0.f)
    {
        roughness = texture(metallicRoughnessMap, uv).g;
        roughness = roughness * roughness;
        metallic = texture(metallicRoughnessMap, uv).b; 
    } else if (uMaterialProps.hasRoughnessMap > 0.f) {
        roughness = texture(roughnessMap, uv).r;
        roughness = roughness * roughness;
        metallic = uniformMetallic;
    } else
    {
        roughness = uniformRoughness;
        metallic = uniformMetallic;
    }
    // Determine the specular color
    // sqrt() because I want to make specular color has stronger tint
    vec3 f0 = mix(vec3(0.04f), albedo.rgb, sqrt(metallic));

    float ao = uMaterialProps.hasAoMap > 0.5f ? texture(aoMap, uv).r : 1.0f;
    ao = pow(ao, 3.0f);

    vec3 viewDir = normalize(-fragmentPos); 

    RenderParams renderParams = {
        albedo.rgb,
        f0,
        roughness,
        metallic,
        normal,
        worldSpaceNormal,
        viewDir,
        vec3(0.f),
        vec3(0.f),
        ao,
        1.0
    };

    vec2 screenTexCoord = gl_FragCoord.xy *.5f / vec2(1280.f, 720.f);
    float ssao = texture(ssaoTex, screenTexCoord).r;

    vec3 color = vec3(0.f);
    // analytical lighting
    color += directLighting(renderParams);
    // image-based-lighting
    color += indirectLighting(renderParams) * ssao;
    // baked lighting
    vec3 bakedLighting = vec3(0.f);
    if (uMaterialProps.hasBakedLighting > 0.5f) 
        bakedLighting = texture(lightMap, uv1).rgb;
    color += bakedLighting * albedo.rgb * ssao;

    // write linear color to HDR Framebuffer
    fragColor = vec4(color, 1.0f);

    // TODO: normalize the depth
    // vec2 clipSpaceXy = (gl_FragCoord.xy / 512.f) * 2.f - vec2(1.f);
    // radialDistance = vec3(length(vec3(clipSpaceXy, gl_FragCoord.z)));
    radialDistance = vec3(length(fragmentPos));
}