#version 450 core

in vec3 n;
in vec3 t;
in vec2 uv;
in vec3 fragmentPos;

out vec4 fragColor;

uniform mat4 view;

uniform float kAmbient;
uniform float kDiffuse;
uniform float kSpecular;

uniform int activeNumDiffuse;
uniform int activeNumSpecular;
uniform int activeNumEmission;

uniform float hasAoMap;
uniform float hasNormalMap;

uniform sampler2D diffuseMaps[6];
uniform sampler2D emissionMaps[2];
uniform sampler2D normalMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;
uniform samplerCube envmap;
uniform samplerCube diffuseIrradianceMap;

uniform int numPointLights;
uniform int numDirLights;

const int aoStrength = 2;

#define pi 3.14159265359

struct Light 
{
    vec4 color;
};

// TODO: Watch out for alignment issues?
struct DirLight
{
    vec4 color;
    vec4 direction;
};

struct PointLight
{
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


// TODO: Handedness ...?
vec4 tangentSpaceToViewSpace(vec3 tn, vec3 vn, vec3 t) 
{
    mat4 tbn;
    vec3 b = cross(vn, t);
    tbn[0] = vec4(t, 0.f);
    tbn[1] = vec4(b, 0.f);
    tbn[2] = vec4(vn, 0.f);
    tbn[3] = vec4(0.f, 0.f, 0.f, 1.f);
    return tbn * vec4(tn, 0.0f);
}

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

// TODO: Rename this or refactor this to handle gamma for both directions
vec3 gammaCorrection(vec3 inColor)
{
    return vec3(pow(inColor.r, 2.2f), pow(inColor.g, 2.2f), pow(inColor.b, 2.2f));
}

vec3 fresnel(vec3 f0, vec3 v, vec3 h)
{
    float fresnelCoef = 1.f - dot(v, h);
    fresnelCoef = fresnelCoef * fresnelCoef * fresnelCoef * fresnelCoef * fresnelCoef;
    return mix(f0, vec3(1.f), fresnelCoef);
}

// Taking the formula from the GGX paper and simplify to avoid computing tangent
// TODO: Why using alpha = roughness * roughness change diffuse color
// Seems like the reason is with alpha = roughness * roughenss, the specular gets darker
float GGX(float roughness, float ndoth) 
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float result = alpha2;
    float denom = ndoth * ndoth * (alpha2 - 1.f) + 1.f;
    result /= (pi * denom * denom); 
    return result;
}

// v: light or view direction, h: half vector
float smithG(vec3 v, vec3 n, vec3 h, float roughness)
{
    float ndotv = dot(n,v);
    float hdotv = dot(h,v);
    if ((hdotv / ndotv) <= 0.f) return 0.f;
    float ndotv2 = ndotv * ndotv;
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float tanTheta2 = (1.f - ndotv2) / ndotv2;
    float result = 2.f / (1.f + sqrt(1.f + alpha2 * tanTheta2));
    return result;
}

vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    vec3 result = (x*(a*x+b))/(x*(c*x+d)+e);
    return vec3(saturate(result.r), saturate(result.g), saturate(result.b)); 
}

vec3 radiance(vec3 albedo, vec3 f0, float roughness, float metallic, float ao, vec3 viewDir, vec3 n, vec3 lc, vec3 ld, float li, float ndotv)
{
    // Diffuse
    float ndotl = max(0.0f, dot(n, ld));
    vec3 diffuse = albedo;

    // Specular
    vec3 h = normalize(ld + viewDir);

    float D = GGX(roughness, max(0.0f, dot(n,h))); // correct; need to examine roughness = 0.0
    float G = smithG(ld, n, h, roughness) * smithG(viewDir, n, h, roughness);
    vec3 f = fresnel(f0, ld, h);
    vec3 specular = (f * D * G) / max(0.00001f, (4.f * ndotv * ndotl));

    vec3 kd = mix(vec3(1.f) - f, vec3(0.0f), metallic);
    return (kDiffuse * kd * diffuse + kSpecular * specular) * ndotl * lc * li * ao;
}

float hash(float seed)
{
    return fract(sin(seed)*43758.5453);
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

void main() 
{
    /* Normal mapping */
    // Interpolation done by the rasterizer may change the length of the normal 
    vec3 normal = normalize(n);
    vec3 tangent = normalize(t);

    if (hasNormalMap > 0.5f)
    {
        vec3 tn = texture(normalMap, uv).xyz;

        // Convert from [0, 1] to [-1.0, 1.0]
        tn = tn * 2.f - vec3(1.f); 

        // Covert normal from tangent frame to camera space
        normal = tangentSpaceToViewSpace(tn, normal, t).xyz;

        // TODO: Re-orthonoramlize the tangent frame
    }

    /* Texture mapping */
    vec4 albedo = texture(diffuseMaps[0], uv);
    albedo.rgb = vec3(pow(albedo.r, 2.2f), pow(albedo.g, 2.2f), pow(albedo.b, 2.2f));

    // According to gltf-2.0 spec, metal is sampled from b, roughness is sampled from g
    float roughness = texture(roughnessMap, uv).g;
    float metallic = texture(roughnessMap, uv).b; 

    // Determine the specular color
    vec3 f0 = mix(vec3(0.04f), albedo.rgb, metallic);

    /* Shading */
    // Ambient
    vec3 ambient = vec3(0.22f, 0.22f, 0.2f);
    float ao = hasAoMap > 0.5f ? texture(aoMap, uv).r : 1.0f;
    ao = pow(ao, aoStrength);
    ambient *= albedo.rgb * ao;

    vec3 viewDir = normalize(-fragmentPos); 
    float ndotv = max(0.0f, dot(normal,viewDir));

    vec3 color = vec3(0.f);
    for (int i = 0; i < numPointLights; i++)
    {
        vec4 pos = view * pointLightsBuffer.lights[i].position;
        vec3 ld = normalize(pos.xyz - fragmentPos);
        vec3 lc = gammaCorrection(pointLightsBuffer.lights[i].color.rgb);
        float li = pointLightsBuffer.lights[i].color.w;
        color += radiance(albedo.rgb, f0, roughness, metallic, ao, viewDir, normal, lc, ld, li, ndotv);
    }

    for (int i = 0; i < numDirLights; i++)
    {
        vec4 dir = view * dirLightsBuffer.lights[i].direction;
        vec3 ld = normalize(-dir.xyz);
        vec3 lc = dirLightsBuffer.lights[i].color.rgb;
        float li = dirLightsBuffer.lights[i].color.w;
        color += radiance(albedo.rgb, f0, roughness, metallic, ao, viewDir, normal, lc, ld, li, ndotv);
    }

    // Image-based-lighting
    {
        // Diffuse
        vec3 f = fresnel(f0, normal, normalize(normal + viewDir));
        vec3 kd = mix(vec3(1.f) - f, vec3(0.f), metallic);
        vec3 diffuseE = texture(diffuseIrradianceMap, normal).rgb;
        color += albedo.rgb * diffuseE * 6.5f * kd;

        // Specular: importance sampling GGX
        vec3 specularE = vec3(0.f);
        float numSamples = 512.f;
        for (int sa = 0; sa < numSamples; sa++)
        {
            // Random samples a microfacet normal following GGX distribution
            float rand_u = hash(sa * 12.3f / numSamples); 
            float rand_v = hash(sa * 78.2f / numSamples); 

            float theta = atan(roughness * sqrt(rand_u) / sqrt(1 - rand_u));
            float phi = 2 * pi * rand_v;
            vec3 h = generateSample(normal, theta, phi);

            // Reflect viewDir against microfacet normal to get incident direction
            vec3 vi = -reflect(viewDir, h);

            // Sample a color
            vec3 envColor = texture(envmap, vi).rgb;

            // Shading
            float shadowing = smithG(vi, normal, h, roughness) * smithG(viewDir, normal, h, roughness);
            vec3 fresnel = fresnel(f0, vi, h); 
            vec3 nom = fresnel * shadowing * saturate(dot(viewDir, h));
            float denom = max(0.0001f, saturate(dot(viewDir, normal)) * saturate(dot(h, normal)));
            specularE += envColor * (nom / denom);
        }
        specularE /= numSamples;
        color += specularE * kSpecular;
    }

/*
    // Emission
    vec3 emission = vec3(0.f); 
    for (int i = 0; i < activeNumEmission; i++)
    {
        vec3 le = texture(emissionMaps[i], uv).rgb;
        le = vec3(pow(le.r, 2.2f), pow(le.g, 2.2f), pow(le.b, 2.2f));
        emission += le;
    }
    color += emission;
*/

    // Tone mapping
    color = ACESFilm(color);
    color = vec3(pow(color.r, 1.f/2.2f), pow(color.g, 1.f/2.2f), pow(color.b, 1.f/2.2f));
    fragColor = vec4(color, 1.0f);
}