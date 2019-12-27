#version 450 core
in float fragment_depth;
in vec3  fragment_normal;
in vec2  fragment_texture_uv;
out vec4 fragment_color;

struct Light {
    vec3 direction;
    float intensity;
};

uniform sampler2D texture_sample;
uniform Light light;

mat4 construct_TBN(vec3 n, vec3 t) {
    mat4 TBN;
    vec3 b = cross(n, t);
    TBN[0] = vec4(t, 0.f);
    TBN[1] = vec4(b, 0.f);
    TBN[2] = vec4(n, 0.f);
    TBN[3] = vec4(0.f, 0.f, 0.f, 1.f);
    return TBN;
}

void main() {
    float diffuse_coef = clamp(dot(fragment_normal, light.direction), 0.f, 1.f);
    fragment_color = texture(texture_sample, fragment_texture_uv) * diffuse_coef;
}