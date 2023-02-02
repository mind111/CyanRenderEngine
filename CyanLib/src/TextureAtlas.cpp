#include "TextureAtlas.h"
#include "CyanRenderer.h"
#include "Image.h"

namespace Cyan
{
    i32 Texture2DAtlas::packImage(const Image& inImage)
    {
        i32 packedImageIndex = -1;

        // make sure the image being packed is a power of 2 square shaped image
        assert(inImage.width == inImage.height && Cyan::isPowerOf2(inImage.width) && inImage.width <= maxSubtextureSize);

        // todo: make sure the format of incoming texture matches with that of the atlas
        Cyan::Texture2D::Spec spec(inImage);
        if (spec.format != atlas->getSpec().format)
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

    i32 Texture2DAtlas::addSubtexture(i32 srcImageIndex, const Sampler2D& inSampler, bool bGenerateMipmap)
    {
        if (srcImageIndex >= 0)
        {
            Subtexture subtexture = { };
            subtexture.src = srcImageIndex;
            subtexture.sampler = inSampler;
            subtextures.push_back(subtexture);
            return subtextures.size() - 1;
        }
        return -1;
    }
}
