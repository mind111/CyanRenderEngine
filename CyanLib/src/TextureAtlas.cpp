#include "TextureAtlas.h"
#include "CyanRenderer.h"
#include "Image.h"

namespace Cyan
{
    static ITexture::Spec getImageSpec(const Cyan::Image& image, bool bGenerateMipmap = false)
    {
        Cyan::ITexture::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = image.width;
        spec.height = image.height;
        spec.pixelData = image.pixels.get();
        if (bGenerateMipmap)
        {
            spec.numMips = log2(glm::min(spec.width, spec.height)) + 1;
        }
        else
        {
            spec.numMips = 0;
        }
        switch (image.bitsPerChannel)
        {
        case 8:
            switch (image.numChannels)
            {
            case 1: spec.pixelFormat = PF_R8; break;
            case 3: spec.pixelFormat = PF_RGB8; break;
            case 4: spec.pixelFormat = PF_RGBA8; break;
            default: assert(0); break;
            }
            break;
        case 16:
            switch (image.numChannels)
            {
            case 1: spec.pixelFormat = PF_R16F; break;
            case 3: spec.pixelFormat = PF_RGB16F; break;
            case 4: spec.pixelFormat = PF_RGBA16F; break;
            default: assert(0); break;
            }
            break;
        case 32:
            switch (image.numChannels)
            {
            case 3: spec.pixelFormat = PF_RGB32F; break;
            case 4: spec.pixelFormat = PF_RGBA32F; break;
            default: assert(0); break;
            }
            break;
        default:
            assert(0);
        }
        return spec;
    }

    i32 Texture2DAtlas::packImage(const Image& inImage)
    {
        i32 packedImageIndex = -1;

        // make sure the image being packed is a power of 2 square shaped image
        assert(inImage.width == inImage.height && Cyan::isPowerOf2(inImage.width) && inImage.width <= maxSubtextureSize);

        // todo: make sure the format of incoming texture matches with that of the atlas
        Cyan::ITexture::Spec spec = getImageSpec(inImage, true);
        if (spec.pixelFormat != atlas->getTextureSpec().pixelFormat)
        {
            assert(0);
        }

        images.push_back(inImage);
        ImageQuadTree::Node* node = nullptr;
        if (imageQuadTree->insert(&images.back(), &node))
        {
            Image& image = images.back();

            // convert image to a texture and generate full mipmap chain
            std::unique_ptr<Cyan::Texture2D> tempTexture = std::make_unique<Cyan::Texture2D>(spec);

            // copy its texture memory to corresponding place in the atlas using a pass-through shader
            auto renderer = Cyan::Renderer::get();
            CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            CreatePS(ps, "BlitPS", SHADER_SOURCE_PATH "blit_p.glsl");
            CreatePixelPipeline(pipeline, "BlitQuad", vs, ps);

            i32 numMips = tempTexture->numMips;
            assert(numMips <= atlas->numMips);
            for (i32 i = 0; i < numMips; ++i)
            {
                u32 atlasMipWidth = atlas->width / pow(2, i);
                u32 atlasMipHeight = atlas->height / pow(2, i);

                std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(Cyan::createRenderTarget(atlasMipWidth, atlasMipHeight));
                renderTarget->setColorBuffer(atlas.get(), 0, i);
                renderTarget->setDrawBuffers({ 0 });
                Viewport viewport = {
                    (node->center.x - node->size * .5f) * atlasMipWidth,
                    (node->center.y - node->size * .5f) * atlasMipHeight,
                    node->size * atlasMipWidth,
                    node->size * atlasMipHeight,
                };
                renderer->drawScreenQuad(renderTarget.get(), viewport, pipeline, [&tempTexture, i](Cyan::VertexShader* vs, Cyan::PixelShader* ps) {
                    ps->setTexture("srcTexture", tempTexture.get());
                    ps->setUniform("mip", i);
                });
            }
            packedImageIndex = images.size() - 1;
        }
        else
        {
            images.pop_back();
        }
        return packedImageIndex;
    }

    i32 Texture2DAtlas::addSubtexture(i32 srcImageIndex, const ITexture::Parameter& params)
    {
        if (srcImageIndex >= 0)
        {
            Subtexture subtexture = { };
            subtexture.src = srcImageIndex;
            subtexture.wrap_s = params.wrap_s;
            subtexture.wrap_t = params.wrap_t;
            subtexture.minFilter = params.minificationFilter;
            subtexture.magFilter = params.magnificationFilter;
            subtextures.push_back(subtexture);
            return subtextures.size() - 1;
        }
        return -1;
    }
}
