#include "Texture.h"
#include "Engine.h"
#include "GfxModule.h"

namespace Cyan
{
    Texture::~Texture()
    {

    }

    Texture::Texture(const char* name)
        : Asset(name)
    {
    }

    Texture2D::Texture2D(const char* name, const GHSampler2D& sampler2D, Image* inImage, bool bGenerateMipmap)
        : Texture(name)
        , m_image(inImage)
    {
        m_image->addListener([this, sampler2D, bGenerateMipmap](Image* image) {
            FrameGfxTask task = { };
            task.debugName = "CreateTexture2D: " + getName();
            task.lambda = [this, image](Frame& frame) {
                assert(image == m_image);
                // todo: create and init GHTexture2D on the render thread 
            };
            Engine::get()->enqueueFrameGfxTask(task);
        });
    }

    Texture2D::~Texture2D()
    {

    }
}