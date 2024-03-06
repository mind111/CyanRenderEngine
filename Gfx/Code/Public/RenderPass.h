#pragma once

#include "Core.h"
#include "Gfx.h"
#include "Shader.h"
#include "GfxHardwareAbstraction/GHInterface/GHTexture.h"
#include "GfxHardwareAbstraction/GHInterface/GHFramebuffer.h"
#include "GfxContext.h"
#include "MathLibrary.h"

namespace Cyan
{
    struct RenderTarget
    {
        enum class Type
        {
            k2D = 0,
            kCube,
            kCount
        };

        RenderTarget() 
            : type(Type::kCount)
            , colorTexture(nullptr)
            , mipLevel(0)
            , bNeedsClear(false)
        {

        }

        RenderTarget(GHTexture2D* inColorTexture, u32 inMipLevel = 0, bool bClear = true)
            : type(Type::k2D)
            , colorTexture(inColorTexture)
            , mipLevel(inMipLevel)
            , bNeedsClear(bClear)
        {

        }

        RenderTarget(GHTextureCube* inColorTexture, u32 faceIndex, u32 inMipLevel = 0, bool bClear = true)
            : type(Type::kCube)
            , colorTexture(inColorTexture)
            , mipLevel(inMipLevel)
            , bNeedsClear(bClear)
            , cubeFaceIndex(faceIndex)
        {

        }

        bool isBound() { return colorTexture != nullptr; }

        u32 width() const
        { 
            glm::ivec2 mipSize;

            switch (type)
            {
            case Type::k2D: {
                GHTexture2D* tex2D = static_cast<GHTexture2D*>(colorTexture);
                tex2D->getMipSize(mipSize.x, mipSize.y, mipLevel);
                break;
            }
            case Type::kCube: {
                GHTextureCube* texCube = static_cast<GHTextureCube*>(colorTexture);
                texCube->getMipSize(mipSize.x, mipLevel);
                mipSize.y = mipSize.x;
                break;
            }
            default:
                break;
            }

            return mipSize.x;
        }

        u32 height() const
        {
            glm::ivec2 mipSize;

            switch (type)
            {
            case Type::k2D: {
                GHTexture2D* tex2D = static_cast<GHTexture2D*>(colorTexture);
                tex2D->getMipSize(mipSize.x, mipSize.y, mipLevel);
                break;
            }
            case Type::kCube: {
                GHTextureCube* texCube = static_cast<GHTextureCube*>(colorTexture);
                texCube->getMipSize(mipSize.x, mipLevel);
                mipSize.y = mipSize.x;
                break;
            }
            default:
                break;
            }

            return mipSize.y;
        }

        Type type = Type::k2D;
        GHTexture* colorTexture = nullptr;
        u32 mipLevel = 0;
        bool bNeedsClear = true;
        glm::vec4 clearColor = glm::vec4(0.f, 0.f, 0.f, 1.f);
        u32 cubeFaceIndex = 0;
    };

    struct DepthTarget
    {
        DepthTarget() = default;
        DepthTarget(GHDepthTexture* inDepthTexture, bool shouldClearDepth = true, f32 depthClearValue = 1.f)
            : depthTexture(inDepthTexture), bNeedsClear(shouldClearDepth), clearValue(depthClearValue)
        {

        }

        u32 width() const
        { 
            const auto& desc = depthTexture->getDesc();
            return desc.width;
        }

        u32 height() const
        {
            const auto& desc = depthTexture->getDesc();
            return desc.height;
        }

        bool isBound() { return depthTexture != nullptr; }

        GHDepthTexture* depthTexture = nullptr;
        bool bNeedsClear = true;
        f32 clearValue = 1.f;
    };

    class GFX_API RenderPass
    {
    public:
        using RenderFunc = std::function<void(GfxContext*)>;

        RenderPass() = delete;
        RenderPass(u32 renderWidth, u32 renderHeight);
        // RenderPass(GHTexture2D* colorbuffer, GHDepthTexture* depthbuffer, bool bClearColor = true, bool bClearDepth = true);
        ~RenderPass();

        void enableRenderToDefaultTarget() { bRenderToDefaultTarget = true; }
        void disableRenderToDefaultTarget() { bRenderToDefaultTarget = false; }

        void setRenderTarget(const RenderTarget& renderTarget, u32 slot);
        void setDepthTarget(const DepthTarget& depthTarget);
        void setViewport(i32 x, i32 y, i32 width, i32 height);

        void enableDepthTest() { bDepthTest = true; }
        void disableDepthTest() { bDepthTest = false; }

        void enableBackfaceCulling() { bBackfaceCulling = true; }
        void disableBackfaceCulling() { bBackfaceCulling = false; }

        void setRenderFunc(const RenderFunc& renderFunc);
        void render(GfxContext* ctx);
    private:
        static GHFramebuffer* createFramebuffer(const RenderPass& pass);
        void setupFramebuffer(GHFramebuffer* framebuffer);
        void cleanupFramebuffer(GHFramebuffer* framebuffer);

        bool bRenderToDefaultTarget = false;
        bool bHasAnyTargetBound = false;

        std::array<RenderTarget, FRAMEBUFFER_MAX_NUM_COLOR_BUFFERS> m_renderTargets;
        DrawBufferBindings m_drawBufferBindings;
        DepthTarget m_depthTarget;
        Viewport m_viewport;
        u32 m_renderWidth = 0, m_renderHeight = 0;
        bool bDepthTest = false;
        bool bBackfaceCulling = true; // backface culling is enbaled by default

        RenderFunc m_renderFunc;
    };

#define IM_DISPATCH_GFX_PASS()
}
