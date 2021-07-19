#version 450 core

in vec3 n;
in vec3 t;
in vec2 uv;
in vec3 fragmentPos;

out vec4 fragColor;

uniform float kAmbient;
uniform float kDiffuse;
uniform float kSpecular;

const bool hasNormalMap = true;

uniform int activeNumDiffuse;
uniform int activeNumSpecular;
uniform int activeNumEmission;

uniform float hasAoMap;

uniform sampler2D diffuseMaps[6];
uniform sampler2D specularMaps[6];
uniform sampler2D emissionMaps[2];
uniform sampler2D normalMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;

//- transforms
uniform mat4 s_view;

const int aoStrength = 2;
const int kMaxSpecularExponent = 512;

#define pi 3.14159

struct DirectionalLight
{
    vec3 direction;
    vec3 color;
};

vec4 tangentSpaceToViewSpace(vec3 nt, vec3 vn, vec3 t) 
{
    mat4 tbn;
    vec3 b = cross(n, t);
    tbn[0] = vec4(t, 0.f);
    tbn[1] = vec4(b, 0.f);
    tbn[2] = vec4(n, 0.f);
    tbn[3] = vec4(0.f, 0.f, 0.f, 1.f);
    return tbn * vec4(nt, 0.0f);
}

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

vec3 gammaCorrection(vec3 inColor)
{
    return vec3(pow(inColor.r, 2.2f), pow(inColor.g, 2.2f), pow(inColor.b, 2.2f));
}

void main() 
{
    /* lights */
    vec3 lightDir = normalize(vec3(0.2f, 0.4f, 0.7f));
    vec3 lightColor = vec3(0.85f, 0.85f, 0.9f);
    // Bring lightColor to linear space
    lightColor = gammaCorrection(lightColor);

    /* Normal mapping */
    vec3 normal = n;
    if (hasNormalMap)
    {
        vec3 tn = texture(normalMap, uv).xyz;
        // Normalize from [0, 1] to [-1.0, 1.0]
        tn = tn * 2.f - vec3(1.f); 
        // Covert normal from tangent frame to world frame
        normal = tangentSpaceToViewSpace(tn, n, t).xyz;
        // TODO: Re-orthonoramlize the tangent frame

    }

    /* Texture mapping */
    for (int t = 0; t < activeNumDiffuse; t++)
    {

    }

    vec4 albedo = texture(diffuseMaps[0], uv);
    albedo.rgb = vec3(pow(albedo.r, 2.2f), pow(albedo.g, 2.2f), pow(albedo.b, 2.2f));

    /* Shading */
    // Ambient
    vec3 ambient = vec3(0.22f, 0.22f, 0.2f);
    float ao = hasAoMap > 0.5f ? texture(aoMap, uv).r : 1.0f;
    ao = pow(ao, aoStrength);
    ambient *= albedo.rgb * ao;

    // Diffuse
    float diffuseCoef = saturate(dot(normal, lightDir));
    vec3 diffuse = albedo.rgb * lightColor * diffuseCoef * ao;

    // Specular
    // According to gltf-2.0 spec, metal is sampled from b, roughness is sampled from g
    float roughness = texture(roughnessMap, uv).g;
    float exponent = (1.f - roughness) * kMaxSpecularExponent;

    vec3 viewDir = normalize(-fragmentPos); 
    vec3 h = normalize(viewDir + lightDir);
    float ndoth = saturate(dot(h, normal));
    // If light come from beneath the surface, we shouldn't see any specular highlight
    float specCoef = ndoth;
    specCoef = pow(specCoef, exponent) * ((exponent + 1.f) / (2.f * pi));
    /* Fresnel */
    float fresnel = 1 - saturate(dot(viewDir, h));
    fresnel = pow(fresnel, 5.0f);
    vec3 specular = mix(albedo.rgb, vec3(1.0f), fresnel) * lightColor;
    specular *= specCoef;

    // Emission
    vec3 emission = vec3(0.f); 
    for (int i = 0; i < activeNumEmission; i++)
    {
        emission += texture(emissionMaps[i], uv).rgb;
    }

    vec3 color = ambient * kAmbient + diffuse * kDiffuse * ao + specular * kSpecular + emission;
    
    color.rgb = vec3(pow(color.r, 1.f/2.2f), pow(color.g, 1.f/2.2f), pow(color.b, 1.f/2.2f));
    fragColor = vec4(color, 1.0f);
}