#include <array>

#include "glm/glm.hpp"
#include "Texture.h"

namespace Cyan
{
    struct ImageQuadTree
    {
        struct Node
        {
            Node(ImageQuadTree* inOwner, Node* inParent, const glm::vec2& inCenter, f32 inSize)
                : owner(inOwner), parent(inParent), center(inCenter), size(inSize)
            {
            }

            bool insert(Image* inImage, Node** outNode)
            {
                static glm::vec2 childOffsets[4] = { 
                    glm::vec2(-.5f, .5f), 
                    glm::vec2( .5f, .5f), 
                    glm::vec2(-.5f,-.5f),
                    glm::vec2( .5f,-.5f),
                };

                i32 nodeSize = (i32)(size * owner->sizeInPixels);
                // found the correct slot to insert
                if (inImage->width == nodeSize)
                {
                    // if current node is a leaf node
                    if (!bChildrenInitialized)
                    {
                        // if current node is available
                        if (image == nullptr)
                        {
                            image = inImage;
                            *outNode = this;
                            return true;
                        }
                        return false;
                    }
                    return false;
                }
                else if (inImage->width < nodeSize)
                {
                    if (!bChildrenInitialized)
                    {
                        for (i32 i = 0; i < 4; ++i)
                        {
                            assert(children[i] == nullptr);
                            f32 childSize = size / 2.f;
                            glm::vec2 childCenter = center + (childOffsets[i] * childSize);
                            children[i] = new Node(owner, this, childCenter, childSize);
                        }
                        bChildrenInitialized = true;
                    }
                    for (i32 i = 0; i < 4; ++i)
                    {
                        if (!children[i]->isFull())
                        {
                            if (children[i]->insert(inImage, outNode))
                            {
                                return true;
                            }
                        }
                    }
                    return false;
                }
                return false;
            }

            bool isFull()
            {
                bool bFull = true;
                // internal node
                if (bChildrenInitialized)
                {
                    for (i32 i = 0; i < children.size(); ++i)
                    {
                        bFull &= children[i]->isFull();
                    }
                }
                // leaf
                else
                {
                    bFull = (image != nullptr);
                }
                return bFull;
            }

            ImageQuadTree* owner = nullptr;
            Node* parent = nullptr;
            glm::vec2 center;
            f32 size;
            bool bChildrenInitialized = false;
            std::array<Node*, 4> children = { nullptr, nullptr, nullptr, nullptr };
            Cyan::Image* image = nullptr;
        };

        ImageQuadTree(u32 inSize)
            : sizeInPixels(inSize)
        {
            root = new Node(this, nullptr, glm::vec2(.5), 1.f);
        }

        bool insert(Image* image, Node** outNode)
        {
            if (root)
            {
                if (root->isFull()) 
                {
                    printf("The atlas is fully filled. Failed to insert image \n");
                    *outNode = nullptr;
                    return false;
                }
                return root->insert(image, outNode);
            }
            return false;
        }

        Node* root = nullptr;
        f32 sizeInPixels;
    };

    struct Texture2DAtlas
    {
        struct Subtexture
        {
            // texcoord transform
            glm::vec2 scale;
            glm::vec2 translate;
            // addressing mode
            ITexture::Parameter::WrapMode wrap_s;
            ITexture::Parameter::WrapMode wrap_t;
            // filtering mode
            ITexture::Parameter::Filtering minFilter;
            ITexture::Parameter::Filtering magFilter;
        };

        Texture2DAtlas(u32 inSizeInPixels, const ITexture::Spec::PixelFormat& inFormat)
        {
            Cyan::ITexture::Spec spec = { };
            spec.type = TEX_2D;
            spec.width = inSizeInPixels;
            spec.height = inSizeInPixels;
            spec.pixelFormat = inFormat;
            spec.numMips = log2(inSizeInPixels) + 1;

            Cyan::ITexture::Parameter params = { };
            params.magnificationFilter = FM_BILINEAR;
            params.minificationFilter = FM_TRILINEAR;
            atlas = std::make_unique<Cyan::Texture2D>(spec, params);
            imageQuadTree = std::make_unique<ImageQuadTree>(atlas->width);
        }

        ~Texture2DAtlas()
        {

        }

        void init();
        bool pack(const Image& inImage);

        const u32 maxSubtextureSize = 4096;
        std::vector<Image> images;
        std::vector<Subtexture> subtextures;
        std::unique_ptr<ImageQuadTree> imageQuadTree;
        std::unique_ptr<Texture2D> atlas = nullptr;
    };
}