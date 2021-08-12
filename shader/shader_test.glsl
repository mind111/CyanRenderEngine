struct Hit {
    float t;
    int objectId;
};

struct TraceInfo {
    float d;
    int objectId;
};


// object id #1
float sdTerrain(vec3 p, float height) {
    return p.y - height;
}

// object id #0
float sdSphere(vec3 p, vec3 c, float r) {
    return length(p - c) - r; 
}

TraceInfo scene(vec3 p) {
    p += vec3(0., -0.1, 0.);
    float sphereDist = 0.8;
    // float numCol = clamp(round(p.x / sphereDist), -3.0, 3.0);
    // float numRow = clamp(round(p.y / sphereDist), -2.0, 2.0);
    // p = p - vec3(numCol * sphereDist, numRow * sphereDist, 0.0);
    float dSphere = sdSphere(p, vec3(0.f, 0.f, -2.0), 0.3f);
    float dTerrain = sdTerrain(p, -0.7f);
    int objectId = -1;
    if (dSphere < dTerrain) {
        objectId = 0; 
    } else {
        objectId = 1; 
    }
    return TraceInfo(min(dSphere, dTerrain), objectId);
}

vec3 normal(vec3 p) {
    vec3 delta = vec3(0.001, 0.0, 0.0);
    float dx = scene(p + delta).d - scene(p - delta).d;
    float dy = scene(p + delta.yxy).d - scene(p - delta.yxy).d;
    float dz = scene(p + delta.yyx).d - scene(p - delta.yyx).d;
    return normalize(vec3(dx, dy, dz));
}

Hit castRay(vec3 ro, vec3 rd) {
    float t = 0.f;
    const uint MAX_ITER = 50u;
    const float maxT = 100.f;
    const float EPISILON = 0.001f; 
    for (uint iter = 0u; iter < MAX_ITER; ++iter) {
        TraceInfo trace = scene(ro + t * rd);
        t += trace.d;
        if (t > maxT) {
            break;
        }
        // hit
        if (trace.d < EPISILON) {
            return Hit(t, trace.objectId);
        }
    }
    return Hit(-1.f, -1);
}

struct DirLight {
    vec3 dir;
    vec3 color;
};

struct PointLight {
    vec3 pos;
    vec3 color;
};

float paintRoughness(vec3 n) {
    float roughness = 0.5 + 0.5 * dot(n, vec3(0., 0., 1.));
    return roughness;
}

float paintMetallic(vec3 n) {
    float k = dot(n, normalize(vec3(0., -0.0, 0.5)));
    k = 0.5 + 0.5 * k;
    float metallic = k;
    return metallic;
}

vec3 computeDiffuse(DirLight light, vec3 n, vec3 albedo) {
    float ndotl = clamp(dot(light.dir, n), 0., 1.);
    return light.color * albedo * ndotl;
}

vec3 computeSpecular(DirLight light, vec3 n, vec3 wo, vec3 specularColor) {
    vec3 h = normalize(wo + light.dir);
    float ndoth = .5 + .5 * dot(n,h);
    // float ndoth = clamp(dot(n,h), 0.f, 1.);
    float ndotv = clamp(dot(n,wo), 0., 1.);
    float ndotl = clamp(dot(n, light.dir), 0., 1.);
    float fresnelCoef = (1.-ndotv) * (1.-ndotv) * (1.-ndotv) * (1.-ndotv) * (1.-ndotv);
    vec3  fresnel = mix(specularColor, vec3(1.00f), fresnelCoef);
    vec3 specular = light.color * fresnel * pow(ndoth, (1.-paintRoughness(n)) * 1024.) * ndotl;
    return specularColor;
}

vec3 pbrDiffuse() {
    return vec3(0.f);
}

vec3 pbrSpecular() {
    return vec3(0.f);
}

vec3 render(vec3 p, vec3 albedo, vec3 wo) {
    vec3 n = normal(p);
    float ndotv = clamp(dot(n, wo), 0.0, 1.0); 
    // lights
    DirLight dLight_0 = DirLight(normalize(vec3(1.f, 1.5, 0.0)), vec3(0.2, 0.6, 1.5)); 
    DirLight dLight_1 = DirLight(normalize(vec3(-1.f, -0.0, 0.0)), vec3(0.0, 0.3, 2.0)); 
    DirLight dLight_2 = DirLight(vec3(0., 0., 1.f), vec3(1.000, 0.412, 0.706));
    vec3 lightDir = normalize(vec3(1., 1.5, 0.2));
    vec3 lightColor = vec3(0.8, 0.8, 0.6);
    float metallic = paintMetallic(n);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    float fresnelCoef = (1.-ndotv) * (1.-ndotv) * (1.-ndotv) * (1.-ndotv) * (1.-ndotv);
    vec3 fresnel = mix(f0, vec3(1.), fresnelCoef);
    vec3 kd = mix(vec3(1.) - fresnel, vec3(0.0), metallic);
    // diffuse
    vec3 diffuse = computeDiffuse(dLight_0, n, albedo);
    diffuse += computeDiffuse(dLight_1, n, albedo);
    diffuse += computeDiffuse(dLight_2, n, albedo);
    // specular
    float specularIntensity = 1.f;
    vec3 specular = computeSpecular(dLight_0, n, wo, specularIntensity * f0);
    // specular += computeSpecular(dLight_1, n, wo, specularIntensity * f0);
    // specular += computeSpecular(dLight_2, n, wo, specularIntensity * f0);
    vec3 color = (kd * diffuse + specular) / max((4. * ndotv), 0.001);
    return specular; 
} 

// procedural sdf rendering of a sky and a terrain
void main() {
    vec3 color = vec3(0.f);
    vec2 pixelDim = 2.0 / iResolution.xy;
    // cam
    vec3 ro = vec3(0.f, 0.f, 1.f);
    vec3 lookAt = vec3(0.f, -0.f, -1.f);
    vec3 cameraForward = normalize(lookAt - ro);
    vec3 cameraRight = cross(cameraForward, vec3(0.0, 1.0, 0.0));
    vec3 cameraUp = cross(cameraRight, cameraForward);
    // super-sampling
    for (uint sx = 0u; sx < 2u; ++sx) {
        for (uint sy = 0u; sy < 2u; ++sy) {
            vec2 uv = 2.f * gl_FragCoord.xy / iResolution.xy - 1.f;
            uv += vec2(sx + 1u, sy + 1u) * 0.5 * pixelDim;
            uv.x *= iResolution.x / iResolution.y; 
            // rotating camera ...?
            float dist = 6.0;
            vec3 rd = normalize(uv.x * cameraRight + uv.y * cameraUp + cameraForward * dist);
            // default background color
            vec3 horizonColor = vec3(0.85f);
            vec3 skyDomeColor = vec3(0.000, 0.249, 1.0000);
            vec3 skyColor = mix(horizonColor, skyDomeColor, pow(rd.y, 0.25));
            // ray marching
            Hit hit = castRay(ro, rd);
            if (hit.t > 0.f) {
                if (hit.objectId == 0) {
                    color += render(ro+rd*hit.t, vec3(0.75), -1.f * rd);
                } else if (hit.objectId == 1) {
                    color += vec3(0.85f, 0.85f, 0.85f);
                }
            } else {
                color += vec3(1.0f + rd.y) * 0.03;
            }
        }
    }
    color = pow(color / 4.f, vec3(0.4545));
    pc_fragColor = vec4(color, 1.f);
}