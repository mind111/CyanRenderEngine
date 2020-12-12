#version 450 core

in vec3 n;
in vec3 t;
in vec2 uv;
out vec4 fragColor;

mat4 construct_TBN(vec3 n, vec3 t) {
    mat4 TBN;
    vec3 b = cross(n, t);
    TBN[0] = vec4(t, 0.f);
    TBN[1] = vec4(b, 0.f);
    TBN[2] = vec4(n, 0.f);
    TBN[3] = vec4(0.f, 0.f, 0.f, 1.f);
    return TBN;
}

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

void main() {
    vec3 ambient = vec3(0.02f, 0.02f, 0.0f);
    float diffuseCoef = saturate(dot(n, vec3(0.f, 0.f, 1.f)));
    vec3 diffuse = vec3(0.9f, 0.9f, 0.9f) * diffuseCoef;
    fragColor = vec4(ambient + diffuse, 1.0f);
}