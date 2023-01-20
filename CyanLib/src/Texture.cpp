#include <stbi/stb_image.h>
#include "Texture.h"
#include "CyanAPI.h"

namespace Cyan
{
    ITexture::ITexture(const char* inName, const Spec& inSpec, Parameter inParams)
        : GpuObject(),
        pixelFormat(inSpec.pixelFormat),
        parameter(inParams),
        numMips(inSpec.numMips),
        pixelData(inSpec.pixelData)
    { 
        u32 nameLen = strlen(inName) + 1;
        if (nameLen <= 1)
        {
            cyanError("Invalid texture name length.");
        }
        name = new char[nameLen];
        strcpy_s(name, sizeof(char) * nameLen, inName);
    }

    Texture2D::Texture2D(const char* inName, const Spec& inSpec, Parameter inParams)
        : ITexture(inName, inSpec, inParams),
        width(inSpec.width),
        height(inSpec.height)
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &glObject);
        glBindTexture(GL_TEXTURE_2D, getGpuObject());
        auto glPixelFormat = translatePixelFormat(pixelFormat);
        glTexImage2D(GL_TEXTURE_2D, 0, glPixelFormat.internalFormat, width, height, 0, glPixelFormat.format, glPixelFormat.type, pixelData);
        glBindTexture(GL_TEXTURE_2D, 0);

        initializeTextureParameters(getGpuObject(), parameter);

        if (numMips > 1u)
        {
            // todo: this is slow!
            glGenerateTextureMipmap(getGpuObject());
        }

#if BINDLESS_TEXTURE
        glHandle = glGetTextureHandleARB(getGpuObject());
#endif
    }
}
