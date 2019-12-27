#version 450 core
in vec3 fragNormal;
in vec3 fragTangent;
in vec3 fragBitangent;
in vec2 textureUV;
in vec3 fragPosView;

out vec4 fragColor;

struct Light {
    vec3 color;
    float intensity;
};

struct DirectionLight {
    Light baseLight;
    vec3 direction;
};

struct PointLight {
    Light baseLight;
    vec3 position;
};

uniform DirectionLight dLight;

// ----- Textures -----
uniform int numDiffuseMaps, numSpecularMaps, hasNormalMap;
uniform sampler2D diffuseSamplers[4];
uniform sampler2D specularSampler;
uniform sampler2D normalSampler;

float remap(float value, float low, float high, float newLow, float newHigh) {
    return (value - low) * (newHigh - newLow) / (high - low) + newLow; 
}

vec3 sampleNormal(vec2 textureCoord) {
    vec3 normal = texture(normalSampler, textureCoord).xyz;
    normal.x = remap(normal.x, 0.0, 1.0, -1.0, 1.0);
    normal.y = remap(normal.y, 0.0, 1.0, -1.0, 1.0);
    normal.z = remap(normal.z, 0.0, 1.0, -1.0, 1.0);
    return normalize(normal);
}

mat3 constructTBN(vec3 normal, vec3 tangent, vec3 bitangent) {
    mat3 TBN;
    TBN[0] = normalize(tangent);
    TBN[1] = normalize(bitangent);
    TBN[2] = normalize(normal);
    return TBN;
}

// TODO: Re-orthonormalize t, b, n after interpolation
// TODO: need to fix specular component
void main() {
    vec3 l = normalize(-dLight.direction);
    vec3 v = normalize(-fragPosView);
    vec3 h = normalize(l + v);
    vec3 normalSample = sampleNormal(textureUV);
    vec3 n = hasNormalMap == 1 ? normalize(constructTBN(fragNormal, fragTangent, fragBitangent) * normalSample) : normalize(fragNormal); 
    float diffuseCoef = max(dot(n, l), 0.f); 
    float shininess = numSpecularMaps == 1 ? texture(specularSampler, textureUV).r : 16.0;
    float specularCoef = shininess < 0.03 ? 0.0 : pow(max(dot(n, h), 0.0), shininess * 255.0);

    vec3 albedo = numDiffuseMaps == 0 ? vec3(1.0, 0.2, 0.5) : vec3(0.0);
    // diffuse
    for (int i = 0; i < numDiffuseMaps; i++) {
        albedo += pow(texture(diffuseSamplers[i], textureUV).rgb, vec3(2.2));
    } 
    fragColor = vec4(pow(diffuseCoef * albedo + specularCoef * 0.4 * dLight.baseLight.color, vec3(0.4545)), 1.0);
}