#version 450 core

in vec3 n;
in vec3 worldSpaceNormal;
in vec3 t;
in vec2 uv;
in vec3 fragmentPos;
in vec4 shadowPos;
in vec3 worldSpacePos;

layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec3 fragmentNormal; 
layout (location = 2) out vec3 fragmentDepth; 
layout (location = 3) out vec3 radialDistance;

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

uniform int activeNumDiffuse;
uniform int activeNumSpecular;
uniform int activeNumEmission;
uniform float hasAoMap;
uniform float hasNormalMap;
uniform float hasRoughnessMap;
uniform float hasMetallicRoughnessMap;
uniform float uniformRoughness;
uniform float uniformMetallic;
uniform int numPointLights; //non-material
uniform int numDirLights;   //non_material
//- samplers
uniform sampler2D diffuseMaps[2];
uniform sampler2D emissionMaps[2];
uniform sampler2D normalMap;
uniform sampler2D roughnessMap;
uniform sampler2D metallicRoughnessMap;
uniform sampler2D aoMap;
uniform samplerCube envmap;             //non-material
uniform samplerCube irradianceDiffuse;  //non-material
uniform samplerCube irradianceSpecular; //non-material
uniform sampler2D   brdfIntegral;
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
uniform sampler2DShadow shadowCascades[4];
uniform mat4 lightView;
uniform mat4 lightProjections[4];

// light field probe
uniform vec3 debugCameraPos;
uniform float enableReflection;
uniform sampler2D octRadianceMap;
uniform sampler2D octRadialDepthMap;
uniform sampler2D octNormalMap;

layout(binding = 0) uniform atomic_uint rayCounter;

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

layout(std430, binding = 1) buffer dirLightsData
{
    DirLight lights[];
} dirLightsBuffer;

layout(std430, binding = 2) buffer pointLightsData
{
    PointLight lights[];
} pointLightsBuffer;

layout(std430, binding = 3) coherent buffer debugOctRayData
{
    int numBoundryPoints;
    vec2 m_octMapTexCoords[];
} debugRayOctData;

layout(std430, binding = 4) coherent buffer debugWorldRayData
{
    float numSegments;
    vec4 m_vertices[];
} debugRayWorldData;

layout(std430, binding = 5) coherent buffer debugBoundryData
{
    float numBoundryPoints;
    vec4 boundryPoints[];
} debugBoundries;

// Returns ±1
vec2 signNotZero(vec2 v) {
    return vec2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
}

/** Assumes that v is a unit vector. The result is an octahedral vector on the [-1, +1] square. */
vec2 octEncode(in vec3 v) {
    float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
    vec2 result = v.xy * (1.0 / l1norm);
    if (v.z < 0.0) {
        result = (1.0 - abs(result.yx)) * signNotZero(result.xy);
    }
    return result;
}

/** Returns a unit vector. Argument o is an octahedral vector packed via octEncode,
    on the [-1, +1] square*/
vec3 octDecode(vec2 o) {
    vec3 v = vec3(o.x, o.y, 1.0 - abs(o.x) - abs(o.y));
    if (v.z < 0.0) {
        v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
    }
    return normalize(v);
}

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
    // TODO: figure out how to use wrap lighting properly
    float ndotlWrap = max(0.1f, (dot(params.n, params.l) + wrap) / (1.f + wrap));
    vec3 F = fresnel(params.f0, params.n, params.v);
    vec3 kDiffuse = mix(vec3(1.f) - F, vec3(0.0f), params.metallic);
    vec3 diffuse = kDiffuse * diffuseBrdf(params.baseColor) * ndotl;
    vec3 specular = specularBrdf(params.l, params.v, params.n, params.roughness, params.f0) * ndotl;
    return (directDiffuseSlider * diffuse * params.ao + directSpecularSlider * specular) * params.li * params.shadow;
}

int computeCascadeIndex()
{
    // determin which cascade to sample
    float t = abs(fragmentPos.z) / (100.f - 0.5f);
    int cascadeIndex = 0;
    for (int i = 0; i < 4; ++i)
    {
        if (t < cascadeIntervals[i])
        {
            cascadeIndex = i;
            break;
        }
    }
    return cascadeIndex;
}

// TODO: slope scaled bias...?
// TODO: proper blending between cascades
// TODO: PCF
float isInShadow()
{
    float shadow = 1.f;

    // determine which cascade to sample
    int cascadeOffset = computeCascadeIndex();

    // compute shadowmap uv
    vec4 worldFragPos = inverse(s_view) * vec4(fragmentPos, 1.f);
    vec4 lightViewFragPos = lightProjections[cascadeOffset] * lightView * worldFragPos; 
    vec2 shadowTexCoords = lightViewFragPos.xy * .5f + vec2(.5f); 

    // bring depth into [0, 1] from [-1, 1]
    float fragmentDepth = lightViewFragPos.z * 0.5f + .5f;

    // clip uv used to sample the shadow map 
    vec2 texelOffset = vec2(1.f) / textureSize(shadowCascades[cascadeOffset], 0);
    vec2 samples[9] = { vec2(-1.f, 1.f), vec2(0.f, 1.f), vec2(1.f, 1.f),
                        vec2(-1.f, 0.f), vec2(0.f, 0.f), vec2(1.f, 0.f),  
                        vec2(-1.f, -1.f),vec2(0.f, -1.f), vec2(1.f, -1.f)
                       };
    float sampleWeights[9] = {
        0.1, 0.1, 0.1,
        0.1, 0.2, 0.1,
        0.1, 0.1, 0.1 
    };
    float results = 0.f;
    for (int s = 0; s < 9; ++s)
    {
        vec2 sampleTexCoords = shadowTexCoords + samples[s] * texelOffset * 1.5f;
        float clipUv = dot(sampleTexCoords, vec2(1.f));
        if (clipUv <= 2.f && clipUv >= 0.f)
        {
            results += sampleWeights[s] * texture(shadowCascades[cascadeOffset], vec3(sampleTexCoords, fragmentDepth)).r;
        }
    }
    shadow = results;
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
        renderParams.shadow = isInShadow();
        vec4 lightDir = s_view * (dirLightsBuffer.lights[i].direction);
        // light dir
        renderParams.l = normalize(lightDir.xyz);
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
    // Note(Min): should use world space normal to sample
    // vec3 diffuse = kDiffuse * params.baseColor * texture(irradianceDiffuse, params.n).rgb; 
    vec3 diffuse = kDiffuse * params.baseColor * texture(irradianceDiffuse, normalize(worldSpaceNormal)).rgb; 
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
    vec3 specular = prefilteredColor * (params.f0 * brdf.r + brdf.g);
    color += indirectDiffuseSlider * diffuse + indirectSpecularSlider * specular;
    return color;
}

float hash(float seed)
{
    return fract(sin(seed)*43758.5453);
}

float drawDebugD(vec3 v, vec3 l, vec3 n, float roughness)
{
    vec3 h = normalize(v+l);
    float ndoth = saturate(dot(n, h));
    return GGX(roughness, ndoth);
}

struct Hit
{
    vec3 p;
    float t;
};

float distanceToIntersection(vec3 ro, vec3 rd, vec3 v) {
    float numer;
    float denom = v.y * rd.z - v.z * rd.y;

    if (abs(denom) > 0.1) {
        numer = ro.y * rd.z - ro.z * rd.y;
    } else {
        // We're in the yz plane; use another one
        numer = ro.x * rd.y - ro.y * rd.x;
        denom = v.x * rd.y - v.y * rd.x;
    }
    return numer / denom;
}

float rayTraceOneProbe(vec3 probeOrigin, vec3 ro, vec3 rd)
{
    float tMax = 50.f;
    int numBoundryPoints = 0;
    vec3 boundryPoints[5] = {
        ro,
        vec3(0.f),
        vec3(0.f),
        vec3(0.f),
        vec3(0.f)
    };
    float distances[3] = { 
        0.f, 0.f, 0.f
    };

    // find intersection of ray with xy, yz, xz plane
    vec3 rxz = vec3(0.f);
    vec3 rxy = vec3(0.f);
    vec3 ryz = vec3(0.f);

    // TODO: deal with the case if rd.x/y/z = 0.f
    float txy = (probeOrigin.z - ro.z) / rd.z;
    if (txy > 0.f && rd.z != 0.f)
    {
        distances[numBoundryPoints] = txy;
        numBoundryPoints++;
    }
    float txz = (probeOrigin.y - ro.y) / rd.y;
    if (txz > 0.f && rd.y != 0.f)
    {
        distances[numBoundryPoints] = txz;
        numBoundryPoints++;
    }
    float tyz = (probeOrigin.x - ro.x) / rd.x;
    if (tyz > 0.f && rd.x != 0.f)
    {
        distances[numBoundryPoints] = tyz;
        numBoundryPoints++;
    }
    if (distances[1] < distances[0])
    {
        float temp = distances[0];
        distances[0] = distances[1];
        distances[1] = temp;
    }
    if (distances[2] < distances[1])
    {
        float temp = distances[1];
        distances[1] = distances[2];
        distances[2] = temp;
    }
    if (distances[2] < distances[0])
    {
        float temp = distances[0];
        distances[0] = distances[2];
        distances[2] = temp;
    }
    for (int i = 0; i < numBoundryPoints; i++)
    {
        boundryPoints[i+1] = ro + distances[i] * rd;
    }
    // include ro, and the infinitely far away point on the ray
    numBoundryPoints += 2;

    // Note(Min): the projection of where the light goes to infinity should also be computed
    boundryPoints[numBoundryPoints-1] = ro + tMax * rd;
    for (int i = 0; i < numBoundryPoints; ++i)
    {
        debugBoundries.boundryPoints[i] = vec4(boundryPoints[i], 1.f);
    }

/*
    // writing debug data
    {
        debugRayOctData.numBoundryPoints = numBoundryPoints;
        debugRayOctData.m_octMapTexCoords[0] = octEncode(normalize(ro - probeOrigin));
        debugRayOctData.m_octMapTexCoords[1] = octEncode(normalize(boundryPoints[0] - probeOrigin));
        debugRayOctData.m_octMapTexCoords[2] = octEncode(normalize(boundryPoints[0] - probeOrigin));
        debugRayOctData.m_octMapTexCoords[3] = octEncode(normalize(boundryPoints[1] - probeOrigin));
        debugRayOctData.m_octMapTexCoords[4] = octEncode(normalize(boundryPoints[1] - probeOrigin));
        debugRayOctData.m_octMapTexCoords[5] = octEncode(normalize(boundryPoints[2] - probeOrigin));
        debugRayOctData.m_octMapTexCoords[6] = octEncode(normalize(boundryPoints[2] - probeOrigin));
        debugRayOctData.m_octMapTexCoords[7] = octEncode(normalize(boundryPoints[3] - probeOrigin));

        // FIXME: I cannot figure out why writing to the ssbo in a for loop is not working for now.
        // for (int i = 1; i < 2; ++i)
        // {
        //     debugRayOctData.m_octMapTexCoords[i*2] = octEncode(normalize(boundryPoints[i] - probeOrigin));
        //     debugRayOctData.m_octMapTexCoords[i*2+1] = octEncode(normalize(boundryPoints[i] - probeOrigin));
        // }
    }
*/
    // FIXME: the ray will miss surfaces that it supposed to hit
    // FIXME: the ray will overshoot a bit after a hit
    float distAlongRay = -1.f;
    debugRayWorldData.numSegments = 100.f;
    int totalNumSteps = 1;
    debugRayWorldData.m_vertices[0] = vec4(ro, 1.f);
    debugRayWorldData.m_vertices[1] = vec4(ro + 100.f * rd, 1.f);

    for (int i = 3; i < numBoundryPoints; ++i)
    {
        int segmentNumSteps = 0; 
        vec2 start = octEncode(normalize(boundryPoints[i-1] - probeOrigin));
        vec2 end = octEncode(normalize(boundryPoints[i] - probeOrigin));
        vec2 texDirection = normalize(end - start);
        float texDistance = length(end - start);
        vec2 texelOffset = vec2(2.f) / textureSize(octRadialDepthMap, 0);
        float stepLength = 10.0f * length(texelOffset);
        float marched = 0.f;

        if (texDistance < 0.001f)
            continue; 

        float distanceProbeToRayBefore = max(0.f, distanceToIntersection(ro - probeOrigin, rd, octDecode(start)));
        for (marched = 0.f; marched < texDistance; marched += stepLength)
        {
            // debugging
            if (totalNumSteps < 48)
            {
                vec2 ss = min(texDistance, stepLength * segmentNumSteps) * texDirection;
                vec3 test = octDecode(start + ss);
                float testDistance = max(0.f, distanceToIntersection(ro - probeOrigin, rd, test));
                debugRayWorldData.m_vertices[totalNumSteps*2] = vec4(probeOrigin, 1.f);
                debugRayWorldData.m_vertices[totalNumSteps*2+1] = vec4(probeOrigin + test * testDistance, 1.f);
            }

            vec2 pBefore = start + min(texDistance, marched) * texDirection;
            // half step
            vec2 pMid = start + min(texDistance, marched + 0.5f * stepLength) * texDirection;
            // full step
            vec2 pAfter = start + min(texDistance, marched + 1.f * stepLength) * texDirection;

            float distanceProbeToSurface = texture(octRadialDepthMap, pMid * .5f + .5f).r;
            vec3 probeToRayDirBefore = octDecode(pBefore); 
            vec3 probeToRayDirMid = octDecode(pMid);
            vec3 probeToRayDirAfter = octDecode(pAfter);

            // distance from p to probe origin after marching current step
            float distanceProbeToRayAfter = max(0.f, distanceToIntersection(ro - probeOrigin, rd, probeToRayDirAfter));
            float minDistanceProbeToRay = min(distanceProbeToRayBefore, distanceProbeToRayAfter);
            float maxDistanceProbeToRay = max(distanceProbeToRayBefore, distanceProbeToRayAfter);

            // TODO: deal with undefined trace results between occluded ray or definitive miss
            // TODO: add thickness heuristic
            if (maxDistanceProbeToRay >= (distanceProbeToSurface) && (minDistanceProbeToRay < distanceProbeToSurface))
            {
                // coarse assumption that the hit happens at the midpoint between previous 
                // probe to ray intersection and current probe to ray intersection
                float distanceToHit = max(0.f, distanceToIntersection(ro-probeOrigin, rd, probeToRayDirMid));

                vec3 hit = distanceToHit * probeToRayDirMid + probeOrigin;

                vec3 before = probeOrigin + distanceProbeToRayBefore * probeToRayDirBefore;
                vec3 after = probeOrigin + distanceProbeToRayAfter * probeToRayDirAfter;

                // debugRayWorldData.m_vertices[4] = vec4(probeOrigin, 1.f);
                // debugRayWorldData.m_vertices[5] = vec4(before, 1.f);
                // debugRayWorldData.m_vertices[6] = vec4(probeOrigin, 1.f);
                // debugRayWorldData.m_vertices[7] = vec4(after, 1.f);
                // debugRayWorldData.m_vertices[8] = vec4(probeOrigin, 1.f);
                // debugRayWorldData.m_vertices[9] = vec4(hit, 1.f);

                // debugRayWorldData.m_vertices[0] = vec4(boundryPoints[i-1], 1.f);
                // debugRayWorldData.m_vertices[1] = vec4(boundryPoints[i], 1.f);
                vec3 startWorld = octDecode(start);
                vec2 ss = stepLength * 80.f * texDirection;
                // debug
                debugBoundries.boundryPoints[0].x = ss.x;
                debugBoundries.boundryPoints[0].y = ss.y;
                debugBoundries.boundryPoints[0].z = texDirection.y;

                // vec3 test = octDecode(start + ss);
                // float startDistance = max(0.f, distanceToIntersection(ro - probeOrigin, rd, startWorld));
                // float testDistance = max(0.f, distanceToIntersection(ro - probeOrigin, rd, test));

                // debugRayWorldData.m_vertices[0] = vec4(ro, 1.f);
                // debugRayWorldData.m_vertices[1] = vec4(ro + 100.f * rd, 1.f);
                // debugRayWorldData.m_vertices[2] = vec4(probeOrigin, 1.f);
                // debugRayWorldData.m_vertices[3] = vec4(probeOrigin + startWorld * startDistance, 1.f);
                // debugRayWorldData.m_vertices[4] = vec4(probeOrigin, 1.f);
                // debugRayWorldData.m_vertices[5] = vec4(probeOrigin + test * testDistance, 1.f);

                return dot(hit-ro, rd);
            }
            distanceProbeToRayBefore = distanceProbeToRayAfter;
            segmentNumSteps++;
            totalNumSteps++;
        }
    }
    return distAlongRay;
}

vec3 debugWorldPosSamples[5] = {
    vec3(-5.79f, 1.55f, 0.02f),
    vec3(-5.79f, -0.11f, 0.02f),
    vec3(-5.79f, -0.11f, 0.02f),
    vec3(-5.79f, -0.11f, 0.02f),
    vec3(-5.79f, -0.11f, 0.02f)
};

void main() 
{
    /* Normal mapping */
    // Interpolation done by the rasterizer may change the length of the normal 
    vec3 normal = normalize(n);
    vec3 tangent = normalize(t);

    if (hasNormalMap > 0.5f)
    {
        vec3 tn = texture(normalMap, uv).xyz;
        // Convert from [0, 1] to [-1.0, 1.0] and renomalize if texture filtering changes the length
        tn = normalize(tn * 2.f - vec3(1.f)); 
        // Covert normal from tangent frame to camera space
        normal = tangentSpaceToViewSpace(tn, normal, tangent).xyz;
    }

    /* Texture mapping */
    vec4 albedo = texture(diffuseMaps[0], uv);
    // from sRGB to linear space
    albedo.rgb = vec3(pow(albedo.r, 2.2f), pow(albedo.g, 2.2f), pow(albedo.b, 2.2f));
    // According to gltf-2.0 spec, metal is sampled from b, roughness is sampled from g
    float roughness, metallic;
    if (hasMetallicRoughnessMap > 0.f)
    {
        roughness = texture(metallicRoughnessMap, uv).g;
        roughness = roughness * roughness;
        metallic = texture(metallicRoughnessMap, uv).b; 
    } else if (hasRoughnessMap > 0.f) {
        roughness = texture(roughnessMap, uv).r;
        roughness = roughness * roughness;
        metallic = 0.0f;
    } else
    {
        roughness = uniformRoughness;
        metallic = uniformMetallic;
    }
    // Determine the specular color
    // sqrt() because I want to make specular color has stronger tint
    vec3 f0 = mix(vec3(0.04f), albedo.rgb, sqrt(metallic));

    float ao = hasAoMap > 0.5f ? texture(aoMap, uv).r : 1.0f;
    ao = pow(ao, 3.0f);

    vec3 viewDir = normalize(-fragmentPos); 

    RenderParams renderParams = {
        albedo.rgb,
        f0,
        roughness,
        metallic,
        normal,
        viewDir,
        vec3(0.f),
        vec3(0.f),
        ao,
        1.0
    };

    vec3 color = vec3(0.f);
    // analytical lighting
    color += directLighting(renderParams);
    // image-based-lighting
    color += indirectLighting(renderParams);
    // color *= ao;

    // experiemental: world space ray-traced reflection using light field probe!!
    vec3 probeOrigin = vec3(-5.f, 1.155f, 0.f);
    if (enableReflection > .5f)
    {
        // write debug view ray data
        uint rayIndex = atomicCounterIncrement(rayCounter);
        // fix camera position for debugging purpose
        if (rayIndex < 1)
        {
            vec3 simulatedViewPos = vec3(-5.f, -.5f, 9.f);
            // simulated view ray
            // debugRayWorldData.m_vertices[rayIndex * 4] = vec4(simulatedViewPos, 1.f);
            // debugRayWorldData.m_vertices[rayIndex * 4 + 1] = vec4(debugWorldPosSamples[rayIndex], 1.f);
            vec3 rv = -reflect(normalize(simulatedViewPos - debugWorldPosSamples[rayIndex]), normalize(vec3(0.5f, 0.f, 0.5f)));
            vec3 ro = probeOrigin + vec3(1.0f, -0.2f, 0.7f);
            vec3 rd = normalize(vec3(-1.f, 1.f, -1.f));
            // float tt = rayTraceOneProbe(probeOrigin, debugWorldPosSamples[rayIndex], rv);
            float tt = rayTraceOneProbe(probeOrigin, ro, rd);
            if (tt > 0.f)
            {
                // reflected view ray
                // debugRayWorldData.m_vertices[rayIndex * 4 + 2] = vec4(debugWorldPosSamples[rayIndex], 1.f);
                // debugRayWorldData.m_vertices[rayIndex * 4 + 3] = vec4(tt * rv + debugWorldPosSamples[rayIndex], 1.f);
            }
        }
    }

    // Emission
    // vec3 emission = vec3(0.f); 
    // for (int i = 0; i < activeNumEmission; i++)
    // {
    //     vec3 le = texture(emissionMaps[i], uv).rgb;
    //     le = vec3(pow(le.r, 2.2f), pow(le.g, 2.2f), pow(le.b, 2.2f));
    //     emission += le;
    // }
    // color += emission;

    // debug view
    // D
    if (debugD > 0.5f)
    {
        vec4 lightPos = s_view * pointLightsBuffer.lights[0].position; 
        vec3 lightDir = normalize(lightPos.xyz - fragmentPos);
        // vec4 lightDir = s_view * dirLightsBuffer.lights[0].direction;
        // vec3 ld = normalize(lightDir.xyz);
        // vec3 debugHalfVector = normalize(viewDir + ld); 
        float D = drawDebugD(viewDir, lightDir, normal, roughness);
        color = vec3(D);
        // color = vec3(GGX(roughness, max(0.0f, dot(normal, debugHalfVector))));
    }
    // F
    if (debugF > 0.5f)
    {
        vec4 lightDir = s_view * (dirLightsBuffer.lights[0].direction * -1.f);
        vec3 ld = normalize(lightDir.xyz);
        color = fresnel(f0, normal, viewDir);
    }
    // G
    if (debugG > 0.5f)
    {
        vec4 lightDir = s_view * (dirLightsBuffer.lights[0].direction * -1.f);
        vec3 ld = normalize(lightDir.xyz);
        float G = ggxSmithG2(viewDir, ld, normal, roughness);
        // float G = ggxSmithG2Ex(viewDir, ld, normal, roughness);
        color = vec3(G);
    }

    int cascade = computeCascadeIndex();
    vec3 shadowDebugColor = vec3(0.f);
    if (cascade == 0)
        shadowDebugColor = vec3(1.f, 0.f, 0.f);
    else if (cascade == 1)
        shadowDebugColor = vec3(0.f, 1.f, 0.f);
    else if (cascade == 2)
        shadowDebugColor = vec3(0.f, 0.f, 1.f);
    else
        shadowDebugColor = vec3(0.7f, 0.7f, 0.7f);

    // debug probe ray tracing
    {
        // vec3 ro = probeOrigin + vec3(1.0f, -0.2f, 0.7f);
        // vec3 rd = normalize(vec3(-1.f, 1.f, -1.f));
        // // debugRayWorldData.m_vertices[0] = vec4(ro, 1.f);
        // float t = rayTraceOneProbe(probeOrigin, ro, rd);
        // vec3 hit = ro + t * rd;
        // // debugRayWorldData.m_vertices[1] = vec4(hit, 1.f);
    }

    // write linear color to HDR Framebuffer
    fragColor = vec4(color, 1.0f);
    // in world space
    fragmentNormal = worldSpaceNormal * 0.5f + vec3(.5f);
    fragmentDepth = vec3(gl_FragCoord.z);
    // TODO: normalize the depth
    // vec2 clipSpaceXy = (gl_FragCoord.xy / 512.f) * 2.f - vec2(1.f);
    // radialDistance = vec3(length(vec3(clipSpaceXy, gl_FragCoord.z)));
    radialDistance = vec3(length(fragmentPos));
}