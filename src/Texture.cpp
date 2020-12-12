#include "stb_image.h"

#include "Texture.h"

unsigned char* TextureUtils::loadImage(const char* filename, int* w, int* h, int* numChannels)
{
    stbi_set_flip_vertically_on_load(true);
    unsigned char* pixels = stbi_load(filename, w, h, numChannels, 0);
    return pixels;
}