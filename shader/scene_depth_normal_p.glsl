#version 450 core
layout (location = 0) out vec3 fDepth;
layout (location = 1) out vec3 fNormal;

in vec3 normalWorld; 
in vec3 tangentWorld;

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

void main()
{
    fDepth = vec3(gl_FragCoord.z);
    fNormal = normalize(normalWorld) * .5f + .5f;
}