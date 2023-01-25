#include "TextureAtlas.h"
#include "CyanRenderer.h"

namespace Cyan
{
    static ITexture::Spec createTexture2DSpec(const Cyan::Image& image, bool bGenerateMipmap = false)
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
            case 1: spec.pixelFormat = PF_R8UI; break;
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

    void Texture2DAtlas::init() { }

    bool Texture2DAtlas::pack(const Cyan::Image& inImage)
    {
        // make sure the image being packed is a power of 2 square shaped image
        assert(inImage.width == inImage.height && Cyan::isPowerOf2(inImage.width) && inImage.width <= maxSubtextureSize);

        // todo: make sure the format of incoming texture matches with that of the atlas
        Cyan::ITexture::Spec spec = createTexture2DSpec(inImage, true);
        if (spec.pixelFormat != atlas->getTextureSpec().pixelFormat)
        {
            printf("Texture format doesn't match, packing failed! \n");
            return false;
        }

        images.push_back(inImage);
        ImageQuadTree::Node* node = nullptr;
        if (imageQuadTree->insert(&images.back(), &node))
        {
            // calculate the texcoord transform for packed texture
            Subtexture subtexture = { };
            subtexture.scale = glm::vec2(node->size);
            subtexture.translate = node->center - node->size * .5f;
            subtexture.wrap_s = WM_WRAP;
            subtexture.wrap_t = WM_WRAP;
            subtexture.minFilter = FM_BILINEAR;
            subtexture.magFilter = FM_TRILINEAR;

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
        }
        else
        {
            images.pop_back();
        }
    }
}
