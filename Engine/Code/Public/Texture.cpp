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

            Engine::get()->enqueueFrameGfxTask(
                RenderingStage::kPreSceneRendering,
                "CreateTexture2D",
                [this, image, sampler2D, bGenerateMipmap](Frame& frame) {
                    assert(image == m_image);
                    // todo: create and init GHTexture2D on the render thread 
                    u32 numMips = 1;
                    if (bGenerateMipmap)
                    {
                        i32 numMipX = glm::log2(image->m_width) + 1;
                        i32 numMipY = glm::log2(image->m_height) + 1;
                        numMips = (u32)glm::min(numMipX, numMipY);
                    }

                    PixelFormat pf = PixelFormat::kCount;
                    switch (image->m_numChannels)
                    {
                    case 1:
                        switch (image->m_bitsPerChannel)
                        {
                        case 8: pf = PF_R8; break;
                        case 16: pf = PF_R16F; break;
                        case 32: pf = PF_R32F; break;
                        }
                        break;
                    case 2:
                        break;
                    case 3:
                        switch (image->m_bitsPerChannel)
                        {
                        case 8: pf = PF_RGB8; break;
                        case 16: pf = PF_RGB16F; break;
                        case 32: pf = PF_RGB32F; break;
                        }
                        break;
                    case 4:
                        switch (image->m_bitsPerChannel)
                        {
                        case 8: pf = PF_RGBA8; break;
                        case 16: pf = PF_RGBA16F; break;
                        case 32: pf = PF_RGBA32F; break;
                        }
                        break;
                    default:
                        assert(0);
                    }

                    GHTexture2D::Desc desc = GHTexture2D::Desc::create(m_image->m_width, m_image->m_height, numMips, pf, image->m_pixels.get());
                    m_GHTexture = GHTexture2D::create(desc, sampler2D);
                }
            );
        });
    }

    Texture2D::~Texture2D()
    {

    }
}