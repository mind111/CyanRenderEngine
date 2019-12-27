#pragma once
#include <string>
#include "glew.h"

struct Texture {
    std::string name, path;
    GLuint textureObj;
};

struct CubemapTexture {
    static enum {
        right, left, top, bottom, back, front
    };
    Texture textures[6];
};