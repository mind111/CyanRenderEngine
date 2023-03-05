#include <iostream>
#include <fstream>
#include <queue>
#include <functional>

#include <glfw/glfw3.h>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/integer.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tiny_gltf/json.hpp>
#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "Common.h"
#include "AssetManager.h"
#include "CyanRenderer.h"
#include "Material.h"
#include "MathUtils.h"
#include "RenderableScene.h"
#include "Lights.h"
#include "LightComponents.h"
#include "IOSystem.h"

namespace Cyan 
{
    HiZBuffer::HiZBuffer(const GfxTexture2D::Spec& inSpec)
        : texture("HiZBuffer", GfxTexture2D::Spec(inSpec.width, inSpec.height, log2(inSpec.width) + 1, inSpec.format), Sampler2D(FM_MIPMAP_POINT, FM_POINT))
    {
        // for now, we should be using square render target with power of 2 resolution
        assert(isPowerOf2(inSpec.width) && isPowerOf2(inSpec.height));
        assert(inSpec.width == inSpec.height);
    }

    void HiZBuffer::build(GfxTexture2D* srcDepthTexture)
    {
        auto renderer = Renderer::get();
        auto gfxTexture = texture.getGfxTexture2D();
        // copy pass
        {
            auto renderTarget = renderer->createCachedRenderTarget("HiZ_Mip[0]", gfxTexture->width, gfxTexture->height);
            renderTarget->setColorBuffer(gfxTexture, 0, 0);
            CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            CreatePS(ps, "HiZCopyPS", SHADER_SOURCE_PATH "hi_z_copy_p.glsl");
            CreatePixelPipeline(pipeline, "HiZCopy", vs, ps);
            renderer->drawFullscreenQuad(
                renderTarget,
                pipeline,
                [srcDepthTexture](VertexShader* vs, PixelShader* ps) {
                    ps->setTexture("srcDepthTexture", srcDepthTexture);
                }
            );
        }
        // building pass
        {
            CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            CreatePS(ps, "BuildHiZPS", SHADER_SOURCE_PATH "build_hi_z_p.glsl");
            CreatePixelPipeline(pipeline, "BuildHiZ", vs, ps);
            u32 mipWidth = gfxTexture->width;
            u32 mipHeight = gfxTexture->height;
            for (i32 i = 1; i < gfxTexture->numMips; ++i)
            {
                mipWidth /= 2u;
                mipHeight /= 2u;
                const i32 kMaxNameLen = 128;
                char name[kMaxNameLen];
                sprintf_s(name, "HiZ_Pass[%d]", i);
                auto src = i - 1;
                auto dst = i;
                auto renderTarget = renderer->createCachedRenderTarget(name, mipWidth, mipHeight);
                renderTarget->setColorBuffer(gfxTexture, 0, (u32)dst);
                renderer->drawFullscreenQuad(
                    renderTarget,
                    pipeline,
                    [this, src, gfxTexture](VertexShader* vs, PixelShader* ps) {
                        ps->setUniform("srcMip", src);
                        ps->setTexture("srcDepthTexture", gfxTexture);
                    }
                );
            }
        }
    }

    GBuffer::GBuffer(const glm::uvec2& resolution)
        : depth("SceneGBufferDepth", GfxTexture2D::Spec(resolution.x, resolution.y, 1, PF_RGB32F), Sampler2D())
        , albedo("SceneGBufferAlbedo", GfxTexture2D::Spec(resolution.x, resolution.y, 1, PF_RGB16F), Sampler2D())
        , normal("SceneGBufferNormal", GfxTexture2D::Spec(resolution.x, resolution.y, 1, PF_RGB32F), Sampler2D())
        , metallicRoughness("SceneGBufferMetallicRoughness", GfxTexture2D::Spec(resolution.x, resolution.y, 1, PF_RGB16F), Sampler2D())
    {

    }

    Renderer* Singleton<Renderer>::singleton = nullptr;
    static StaticMesh* fullscreenQuad = nullptr;

    GfxTexture2D* Renderer::createRenderTexture(const char* textureName, const GfxTexture2D::Spec& inSpec, const Sampler2D& inSampler)
    {
        auto outTexture = GfxTexture2D::create(inSpec, inSampler);
        renderTextures.push_back(outTexture);
        return outTexture;
    }

    Renderer::SceneTextures* Renderer::SceneTextures::create(const glm::uvec2& inResolution)
    {
        static std::unique_ptr<SceneTextures> s_sceneTextures = nullptr;
        if (s_sceneTextures == nullptr)
        {
            s_sceneTextures.reset(new SceneTextures(inResolution));
        }
        else
        {
            if (inResolution != s_sceneTextures->resolution)
            {
                assert(0);
            }
        }
        return s_sceneTextures.get();
    }

    Renderer::SceneTextures::SceneTextures(const glm::uvec2& inResolution)
        : resolution(inResolution)
        , gBuffer(inResolution)
        , HiZ(gBuffer.depth.getGfxTexture2D()->getSpec())
        , directLighting("SceneDirectLighting", GfxTexture2D::Spec(inResolution.x, inResolution.y, 1, PF_RGBA16F), Sampler2D())
        , directDiffuseLighting("SceneDirectDiffuseLighting", GfxTexture2D::Spec(inResolution.x, inResolution.y, 1, PF_RGBA16F), Sampler2D())
        , indirectLighting("SceneIndirectLighting", GfxTexture2D::Spec(inResolution.x, inResolution.y, 1, PF_RGB16F), Sampler2D())
        , ssgiMirror("SSGIMirrorDebug", GfxTexture2D::Spec(inResolution.x, inResolution.y, 1, PF_RGB16F), Sampler2D())
        , ao("SSGIAO", GfxTexture2D::Spec(inResolution.x, inResolution.y, 1, PF_RGB16F), Sampler2D())
        , bentNormal("SSGIBentNormal", GfxTexture2D::Spec(inResolution.x, inResolution.y, 1, PF_RGB16F), Sampler2D())
        , irradiance("SSGIIrradiance", GfxTexture2D::Spec(inResolution.x, inResolution.y, 1, PF_RGB16F), Sampler2D())
        , color("SceneColor", GfxTexture2D::Spec(inResolution.x, inResolution.y, 1, PF_RGB16F), Sampler2D())
    {
        auto renderer = Renderer::get();
        // render target
        renderTarget = renderer->createCachedRenderTarget("Scene", resolution.x, resolution.y);
        // scene color
        Sampler2D sampler;
        // Hi-Z
        {
            /// HiZ = new HiZBuffer(gBuffer.depth.getGfxTexture2D()->getSpec());
            // GfxTexture2D::Spec spec(resolution.x, resolution.y, 1, PF_RGB16F);
        }

        renderer->registerVisualization(std::string("SceneTextures"), "SceneColor", color.getGfxTexture2D());
        renderer->registerVisualization(std::string("SceneTextures"), "SceneAlbedo", gBuffer.albedo.getGfxTexture2D());
        renderer->registerVisualization(std::string("SceneTextures"), "SceneMetallicRoughness", gBuffer.metallicRoughness.getGfxTexture2D());
        renderer->registerVisualization(std::string("SceneTextures"), "SceneDepth", gBuffer.depth.getGfxTexture2D());
        renderer->registerVisualization(std::string("SceneTextures"), "HiZ", HiZ.texture.getGfxTexture2D());
        renderer->registerVisualization(std::string("SceneTextures"), "SSGIMirror", ssgiMirror.getGfxTexture2D());
        renderer->registerVisualization(std::string("SceneTextures"), "SceneNormal", gBuffer.normal.getGfxTexture2D());
        renderer->registerVisualization(std::string("SceneTextures"), "SceneDirectDiffuseLighting", directDiffuseLighting.getGfxTexture2D());
        renderer->registerVisualization(std::string("SceneTextures"), "SceneDirectLighting", directLighting.getGfxTexture2D());
        renderer->registerVisualization(std::string("SceneTextures"), "SceneIndirectLighting", indirectLighting.getGfxTexture2D());
        renderer->registerVisualization(std::string("SceneTextures"), "SSAO", ao.getGfxTexture2D());
        renderer->registerVisualization(std::string("SceneTextures"), "BentNormal", bentNormal.getGfxTexture2D());
        renderer->registerVisualization(std::string("SceneTextures"), "Irradiance", irradiance.getGfxTexture2D());
    }

    Renderer::Renderer(GfxContext* ctx, u32 windowWidth, u32 windowHeight)
        : m_ctx(ctx),
        m_windowSize(windowWidth, windowHeight),
        m_frameAllocator(1024 * 1024 * 32)
    {
    }

    void Renderer::initialize() 
    {
    }

    void Renderer::deinitialize() 
    {
        // imgui clean up
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    // managing creating and recycling render target
    RenderTarget* Renderer::createCachedRenderTarget(const char* name, u32 width, u32 height)
    {
        static std::unordered_map<std::string, std::unique_ptr<RenderTarget>> renderTargetMap;
        std::string suffix = '_' + std::to_string(width) + 'x' + std::to_string(height);
        std::string key = name + suffix;
        auto entry = renderTargetMap.find(key);
        if (entry == renderTargetMap.end())
        {
            RenderTarget* newRenderTarget = createRenderTarget(width, height);
            renderTargetMap.insert({ key, std::unique_ptr<RenderTarget>(newRenderTarget)});
            return newRenderTarget;
        }
        else
        {
            return entry->second.get();
        }
    }

    void Renderer::drawMesh(RenderTarget* renderTarget, Viewport viewport, StaticMesh* mesh, PixelPipeline* pipeline, const RenderSetupLambda& renderSetupLambda, const GfxPipelineConfig& config) 
    {
        for (u32 i = 0; i < mesh->numSubmeshes(); ++i) 
        {
            RenderTask task = { };
            task.renderTarget = renderTarget;
            task.viewport = viewport;
            task.config = config;
            task.pipeline = pipeline;
            task.submesh = mesh->getSubmesh(i);
            task.renderSetupLambda = renderSetupLambda;
            submitRenderTask(task);
        }
    }

    void Renderer::drawFullscreenQuad(RenderTarget* renderTarget, PixelPipeline* pipeline, const RenderSetupLambda& renderSetupLambda) 
    {
        GfxPipelineConfig config;
        config.depth = DepthControl::kDisable;

        drawMesh(
            renderTarget,
            { 0, 0, renderTarget->width, renderTarget->height },
            AssetManager::getAsset<StaticMesh>("FullScreenQuadMesh"),
            pipeline,
            renderSetupLambda,
            config
        );
    }

    void Renderer::drawScreenQuad(RenderTarget* renderTarget, Viewport viewport, PixelPipeline* pipeline, RenderSetupLambda&& renderSetupLambda) 
    {
        GfxPipelineConfig config;
        config.depth = DepthControl::kDisable;

        drawMesh(
            renderTarget,
            viewport,
            AssetManager::getAsset<StaticMesh>("FullScreenQuadMesh"),
            pipeline,
            renderSetupLambda,
            config
        );
    }

    void Renderer::drawColoredScreenSpaceQuad(RenderTarget* renderTarget, const glm::vec2& screenSpaceMin, const glm::vec2& screenSpaceMax, const glm::vec4& color)
    {
        GfxPipelineConfig config;
        config.depth = DepthControl::kDisable;

        // calc viewport based on quad min and max in screen space
        Viewport viewport = { };
        viewport.x = screenSpaceMin.x * renderTarget->width;
        viewport.y = screenSpaceMin.y * renderTarget->height;
        viewport.width = (screenSpaceMax.x - screenSpaceMin.x) * renderTarget->width;
        viewport.height = (screenSpaceMax.y - screenSpaceMin.y) * renderTarget->height;

        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "ColoredQuadPS", SHADER_SOURCE_PATH "colored_quad_p.glsl");
        CreatePixelPipeline(pipeline, "ColoredQuad", vs, ps);

        drawMesh(
            renderTarget,
            viewport,
            AssetManager::getAsset<StaticMesh>("FullScreenQuadMesh"),
            pipeline,
            [color](VertexShader* vs, PixelShader* ps) {
                ps->setUniform("color", color);
            },
            config
        );
    }

    void Renderer::submitRenderTask(const RenderTask& task) 
    {
        // set render target assuming that render target is already properly setup
        m_ctx->setRenderTarget(task.renderTarget);
        m_ctx->setViewport(task.viewport);

        // set shader
        m_ctx->setPixelPipeline(task.pipeline, task.renderSetupLambda);

        // set graphics pipeline state
        m_ctx->setDepthControl(task.config.depth);
        m_ctx->setPrimitiveType(task.config.primitiveMode);

        // kick off the draw call
        auto va = task.submesh->getVertexArray();
        m_ctx->setVertexArray(va);
        m_ctx->drawIndex(task.submesh->numIndices());
    }

    void Renderer::registerVisualization(const std::string& categoryName, const char* visName, GfxTexture2D* visualization, bool* toggle) 
    {
        auto entry = visualizationMap.find(categoryName);
        if (entry == visualizationMap.end()) 
        {
            visualizationMap.insert({ categoryName, { VisualizationDesc{ visName, visualization, 0, toggle } } });
        }
        else 
        {
            entry->second.push_back(VisualizationDesc{ visName, visualization, 0, toggle });
        }
    }

    void Renderer::beginRender() 
    {
        // reset frame allocator
        m_frameAllocator.reset();
    }

    void Renderer::render(Scene* scene, const SceneView& sceneView) 
    {
        beginRender();
        {
            // shared render target for this frame
            m_sceneTextures = SceneTextures::create(glm::uvec2(sceneView.canvas->width, sceneView.canvas->height));

            // render shadow maps first 
            renderShadowMaps(scene);

            // build scene rendering data
            RenderableScene renderableScene(scene, sceneView);

            // depth prepass
            renderSceneDepthPrepass(renderableScene, m_sceneTextures->renderTarget, m_sceneTextures->gBuffer.depth);

            // build Hi-Z buffer
            m_sceneTextures->HiZ.build(m_sceneTextures->gBuffer.depth.getGfxTexture2D());

            // main scene pass
#if BINDLESS_TEXTURE
            renderSceneGBuffer(m_sceneTextures->renderTarget, renderableScene, m_sceneTextures->gBuffer);
#else
            renderSceneGBufferWithTextureAtlas(m_sceneTextures.renderTarget, renderableScene, m_sceneTextures.gBuffer);
#endif
            renderSceneLighting(m_sceneTextures->color, renderableScene, m_sceneTextures->gBuffer);

            // draw debug objects if any
            drawDebugObjects();

            // post processing
            if (m_settings.bPostProcessing) 
            {
                auto bloomTexture = bloom(m_sceneTextures->color.getGfxTexture2D());
                compose(sceneView.canvas, m_sceneTextures->color.getGfxTexture2D(), bloomTexture.getGfxTexture2D(), m_windowSize);
            }

            if (m_visualization)
            {
                visualize(sceneView.canvas, m_visualization->texture, m_visualization->activeMip);
            }
        } 
        endRender();
    }

    void Renderer::visualize(GfxTexture2D* dst, GfxTexture2D* src, i32 mip) 
    {
        auto renderTarget = createCachedRenderTarget("Visualization", dst->width, dst->height);
        renderTarget->setColorBuffer(dst, 0);
        renderTarget->setDrawBuffers({ 0 });
        renderTarget->clearDrawBuffer(0, glm::vec4(.0f, .0f, .0f, 1.f));

        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "BlitPS", SHADER_SOURCE_PATH "blit_p.glsl");
        CreatePixelPipeline(pipeline, "BlitQuad", vs, ps);
        drawFullscreenQuad(
            renderTarget,
            pipeline,
            [this, src, mip](VertexShader* vs, PixelShader* ps) {
                ps->setTexture("srcTexture", src);
                ps->setUniform("mip", mip);
            });
    }

    void Renderer::blitTexture(GfxTexture2D* dst, GfxTexture2D* src)
    {
        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "BlitPS", SHADER_SOURCE_PATH "blit_p.glsl");
        CreatePixelPipeline(pipeline, "BlitQuad", vs, ps);

        auto renderTarget = createCachedRenderTarget("BlitTexture", dst->width, dst->height);
        renderTarget->setColorBuffer(dst, 0);
        renderTarget->setDrawBuffers({ 0 });
        renderTarget->clearDrawBuffer(0, glm::vec4(.0f, .0f, .0f, 1.f));

        // final blit to default framebuffer
        drawFullscreenQuad(
            renderTarget,
            pipeline,
            [this, src](VertexShader* vs, PixelShader* ps) {
                ps->setTexture("srcTexture", src);
                ps->setUniform("mip", (i32)0);
            }
        );
    }

    void Renderer::renderToScreen(GfxTexture2D* inTexture)
    {
        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "BlitPS", SHADER_SOURCE_PATH "blit_p.glsl");
        CreatePixelPipeline(pipeline, "BlitQuad", vs, ps);

        // final blit to default framebuffer
        drawFullscreenQuad(
            RenderTarget::getDefaultRenderTarget(m_windowSize.x, m_windowSize.y),
            pipeline,
            [this, inTexture](VertexShader* vs, PixelShader* ps) {
                ps->setTexture("srcTexture", inTexture);
                ps->setUniform("mip", (i32)0);
            }
        );
    }

    void Renderer::endRender() 
    {
    }

    void Renderer::renderShadowMaps(Scene* inScene) 
    {
        if (inScene->m_directionalLight)
        {
            inScene->m_directionalLight->getDirectionalLightComponent()->getDirectionalLight()->renderShadowMap(inScene, this);
        }
        // todo: other types of shadow casting lights
        for (auto lightComponent : inScene->m_lightComponents)
        {
        }
    }

    void Renderer::renderSceneDepthPrepass(const RenderableScene& scene, RenderTarget* outRenderTarget, RenderTexture2D outDepthTexture)
    {
        GfxTexture2D* outGfxDepthTexture = outDepthTexture.getGfxTexture2D();

        scene.bind(m_ctx);

        outRenderTarget->setColorBuffer(outGfxDepthTexture, 0);
        outRenderTarget->setDrawBuffers({ 0 });
        outRenderTarget->clearDrawBuffer(0, glm::vec4(1.f, 1.f, 1.f, 1.f));

        m_ctx->setRenderTarget(outRenderTarget);
        m_ctx->setViewport({ 0, 0, outRenderTarget->width, outRenderTarget->height });
        CreateVS(vs, "SceneGBufferPassVS", SHADER_SOURCE_PATH "scene_pass_v.glsl");
        CreatePS(ps, "SceneDepthPrepassPS", SHADER_SOURCE_PATH "scene_depth_prepass_p.glsl");
        CreatePixelPipeline(pipeline, "SceneDepthPrepass", vs, ps);
        m_ctx->setPixelPipeline(pipeline);
        m_ctx->setDepthControl(DepthControl::kEnable);
        m_ctx->multiDrawArrayIndirect(scene.indirectDrawBuffer.get());
    }

    void Renderer::renderSceneDepthOnly(RenderableScene& scene, GfxDepthTexture2D* outDepthTexture)
    {
        scene.bind(m_ctx);

        std::unique_ptr<RenderTarget> depthRenderTarget(createDepthOnlyRenderTarget(outDepthTexture->width, outDepthTexture->height));
        depthRenderTarget->setDepthBuffer(outDepthTexture);
        depthRenderTarget->clear({ { 0u } });
        m_ctx->setRenderTarget(depthRenderTarget.get());
        m_ctx->setViewport({ 0, 0, depthRenderTarget->width, depthRenderTarget->height });

        CreateVS(vs, "SceneGBufferPassVS", SHADER_SOURCE_PATH "scene_pass_v.glsl");
        CreatePS(ps, "DepthOnlyPS", SHADER_SOURCE_PATH "depth_only_p.glsl");
        CreatePixelPipeline(pipeline, "DepthOnly", vs, ps);
        m_ctx->setPixelPipeline(pipeline);

        m_ctx->setDepthControl(DepthControl::kEnable);
        m_ctx->multiDrawArrayIndirect(scene.indirectDrawBuffer.get());
    }

    void Renderer::renderSceneGBuffer(RenderTarget* outRenderTarget, const RenderableScene& scene, GBuffer gBuffer)
    {
        // todo: maybe it's worth moving those shader storage buffer cached in "scene" into each function as a
        // static variable, and every time this function is called, reset the data of those persistent gpu buffers

        scene.bind(m_ctx);

        CreateVS(vs, "SceneGBufferPassVS", SHADER_SOURCE_PATH "scene_pass_v.glsl");
        CreatePS(ps, "SceneGBufferPassPS", SHADER_SOURCE_PATH "scene_gbuffer_p.glsl");
        CreatePixelPipeline(pipeline, "SceneGBufferPass", vs, ps);
        m_ctx->setPixelPipeline(pipeline);

        outRenderTarget->setColorBuffer(gBuffer.albedo.getGfxTexture2D(), 0);
        outRenderTarget->setColorBuffer(gBuffer.normal.getGfxTexture2D(), 1);
        outRenderTarget->setColorBuffer(gBuffer.metallicRoughness.getGfxTexture2D(), 2);
        outRenderTarget->setDrawBuffers({ 0, 1, 2 });
        outRenderTarget->clearDrawBuffer(0, glm::vec4(0.f, 0.f, 0.f, 1.f), false);
        outRenderTarget->clearDrawBuffer(1, glm::vec4(0.f, 0.f, 0.f, 1.f), false);
        outRenderTarget->clearDrawBuffer(2, glm::vec4(0.f, 0.f, 0.f, 1.f), false);

        m_ctx->setRenderTarget(outRenderTarget);
        m_ctx->setViewport({ 0, 0, outRenderTarget->width, outRenderTarget->height });
        m_ctx->setDepthControl(DepthControl::kEnable);
        m_ctx->multiDrawArrayIndirect(scene.indirectDrawBuffer.get());
    }

    void Renderer::renderSceneLighting(RenderTexture2D outSceneColor, const RenderableScene& scene, GBuffer gBuffer)
    {
        renderSceneDirectLighting(m_sceneTextures->directLighting, scene, m_sceneTextures->gBuffer);
        renderSceneIndirectLighting(m_sceneTextures->indirectLighting, scene, m_sceneTextures->gBuffer);

        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "SceneLightingPassPS", SHADER_SOURCE_PATH "scene_lighting_p.glsl");
        CreatePixelPipeline(pipeline, "SceneLightingPass", vs, ps);

        auto outSceneColorGfxTexture = outSceneColor.getGfxTexture2D();
        auto renderTarget = createCachedRenderTarget("SceneLightingRenderTarget", outSceneColorGfxTexture->width, outSceneColorGfxTexture->height);
        renderTarget->setColorBuffer(m_sceneTextures->color.getGfxTexture2D(), 0);
        renderTarget->setDrawBuffers({ 0 });
        renderTarget->clearDrawBuffer(0, glm::vec4(0.f, 0.f, 0.f, 1.f), false);

        // combine direct lighting with indirect lighting
        drawFullscreenQuad(
            renderTarget,
            pipeline,
            [this](VertexShader* vs, PixelShader* ps) {
                ps->setTexture("directLightingTexture", m_sceneTextures->directLighting.getGfxTexture2D());
                ps->setTexture("indirectLightingTexture", m_sceneTextures->indirectLighting.getGfxTexture2D());
            }
        );
    }

    void Renderer::renderSceneDirectLighting(RenderTexture2D outDirectLighting, const RenderableScene& scene, GBuffer gBuffer)
    {
        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "SceneDirectLightingPS", SHADER_SOURCE_PATH "scene_direct_lighting_p.glsl");
        CreatePixelPipeline(pipeline, "SceneDirectLightingPass", vs, ps);

        auto outTexture = outDirectLighting.getGfxTexture2D();
        // todo: the rendered depth buffer is coupled with this m_sceneTextures->renderTarget, so need to implement proper depth buffer binding/unbinding
        auto renderTarget = m_sceneTextures->renderTarget;
        renderTarget->setColorBuffer(outTexture, 0);
        renderTarget->setColorBuffer(m_sceneTextures->directDiffuseLighting.getGfxTexture2D(), 1);
        renderTarget->setDrawBuffers({ 0, 1 });
        renderTarget->clearDrawBuffer({ 0 }, glm::vec4(0.f, 0.f, 0.f, 1.f), false);
        renderTarget->clearDrawBuffer({ 1 }, glm::vec4(0.f, 0.f, 0.f, 1.f), false);

        drawFullscreenQuad(renderTarget, pipeline, [gBuffer](VertexShader* vs, PixelShader* ps) {
            ps->setTexture("sceneDepth", gBuffer.depth.getGfxTexture2D());
            ps->setTexture("sceneNormal", gBuffer.normal.getGfxTexture2D());
            ps->setTexture("sceneAlbedo", gBuffer.albedo.getGfxTexture2D());
            ps->setTexture("sceneMetallicRoughness", gBuffer.metallicRoughness.getGfxTexture2D());
        });

        if (scene.skybox)
        {
            scene.skybox->render(m_sceneTextures->renderTarget, scene.view.view, scene.view.projection);
        }
    }

    void Renderer::renderSceneIndirectLighting(RenderTexture2D outIndirectLighting, const RenderableScene& scene, GBuffer gBuffer)
    {
        auto outTexture = outIndirectLighting.getGfxTexture2D();

        // render AO and bent normal, as well as indirect irradiance
        if (bDebugSSRT)
        {
            visualizeSSRT(gBuffer.depth.getGfxTexture2D(), gBuffer.normal.getGfxTexture2D());
        }

        auto SSGI = SSGI::create(glm::uvec2(outTexture->width, outTexture->height));
        SSGI->renderEx(m_sceneTextures->ao, m_sceneTextures->bentNormal, m_sceneTextures->irradiance, m_sceneTextures->gBuffer, m_sceneTextures->HiZ, m_sceneTextures->directDiffuseLighting);

        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "SceneIndirectLightingPass", SHADER_SOURCE_PATH "scene_indirect_lighting_p.glsl");
        CreatePixelPipeline(pipeline, "SceneIndirectLightingPass", vs, ps);

        auto renderTarget = createCachedRenderTarget("SceneIndirectLighting", outTexture->width, outTexture->height);
        renderTarget->setColorBuffer(outTexture, 0);
        renderTarget->setDrawBuffers({ 0 });
        renderTarget->clearDrawBuffer(0, glm::vec4(0.f, 0.f, 0.f, 1.f), false);

        drawFullscreenQuad(renderTarget, pipeline, [this, &scene, gBuffer](VertexShader* vs, PixelShader* ps) {
            ps->setTexture("sceneDepth", gBuffer.depth.getGfxTexture2D());
            ps->setTexture("sceneNormal", gBuffer.normal.getGfxTexture2D());
            ps->setTexture("sceneAlbedo", gBuffer.albedo.getGfxTexture2D());
            ps->setTexture("sceneMetallicRoughness", gBuffer.metallicRoughness.getGfxTexture2D());

            // setup ssao
            ps->setTexture("ssao", m_sceneTextures->ao.getGfxTexture2D());
            ps->setUniform("ssaoEnabled", m_settings.bSSAOEnabled ? 1.f : 0.f);

            // setup ssbn
            ps->setTexture("ssbn", m_sceneTextures->bentNormal.getGfxTexture2D());
            ps->setUniform("ssbnEnabled", m_settings.bBentNormalEnabled ? 1.f : 0.f);

            // setup indirect irradiance
            ps->setTexture("indirectIrradiance", m_sceneTextures->irradiance.getGfxTexture2D());
            ps->setUniform("indirectIrradianceEnabled", m_settings.bIndirectIrradianceEnabled ? 1.f : 0.f);

            // sky light
            /* note
            * seamless cubemap doesn't work with bindless textures that's accessed using a texture handle,
            * so falling back to normal way of binding textures here.
            */
            if (scene.skyLight)
            {
                ps->setTexture("skyLight.irradiance", scene.skyLight->irradianceProbe->m_convolvedIrradianceTexture.get());
                ps->setTexture("skyLight.reflection", scene.skyLight->reflectionProbe->m_convolvedReflectionTexture.get());
                auto BRDFLookupTexture = ReflectionProbe::getBRDFLookupTexture()->glHandle;
                if (glIsTextureHandleResidentARB(BRDFLookupTexture) == GL_FALSE) 
                {
                    glMakeTextureHandleResidentARB(BRDFLookupTexture);
                }
                ps->setUniform("skyLight.BRDFLookupTexture", BRDFLookupTexture);
            }
        });
    }

    // todo: move this into SSGI
    void Renderer::visualizeSSRT(GfxTexture2D* depth, GfxTexture2D* normal)
    {
        // build Hi-Z buffer
        {
            m_sceneTextures->HiZ.build(depth);
        }

        struct DebugTraceData
        {
            f32 minDepth;
            f32 stepDepth;
            f32 tDepth;
            f32 tCellBoundry;
            f32 t;
            i32 level;
            i32 iteration;
            i32 stepCount;
            glm::vec4 cellBoundry;
            glm::vec2 cell;
            glm::vec2 mipSize;
            glm::vec4 pp;
            glm::vec4 ro;
            glm::vec4 rd;
        };

        static std::vector<DebugTraceData> debugTraces(kNumIterations);
        static std::vector<Vertex> debugRays(numDebugRays * kNumIterations);
        static ShaderStorageBuffer debugTraceBuffer("DebugTraceBuffer", sizeOfVector(debugTraces));
        static ShaderStorageBuffer debugRayBuffer("DebugRayBuffer", sizeOfVector(debugRays));

        // debug trace
        {
            CreateCS(cs, "SSRTRayDebugCS", SHADER_SOURCE_PATH "ssrt_ray_debug_c.glsl");
            CreateComputePipeline(pipeline, "SSRTRayDebug", cs);
            m_ctx->setShaderStorageBuffer(&debugTraceBuffer);
            m_ctx->setShaderStorageBuffer(&debugRayBuffer);
            m_ctx->setComputePipeline(pipeline, [this, depth, normal](ComputeShader* cs) {
                    cs->setUniform("debugCoord", debugCoord);
                    cs->setTexture("depthBuffer", depth);
                    cs->setTexture("normalBuffer", normal);
                    cs->setTexture("HiZ", m_sceneTextures->HiZ.texture.getGfxTexture2D());
                    auto blueNoiseTexture = AssetManager::getAsset<Texture2D>("BlueNoise_1024x1024");
                    cs->setTexture("blueNoiseTexture", blueNoiseTexture->gfxTexture.get());
                    cs->setUniform("outputSize", glm::vec2(depth->width, depth->height));
                    cs->setUniform("numLevels", (i32)m_sceneTextures->HiZ.texture.getGfxTexture2D()->numMips);
                    cs->setUniform("kMaxNumIterations", kNumIterations);
                    cs->setUniform("numRays", numDebugRays);
                }
            );
            glDispatchCompute(1, 1, 1);
        }
        // read back data from gpu (slow!!!)
        {
            if (!bFixDebugRay)
            {
                // read back ray data
                // log debug trace data
                memset(debugTraces.data(), 0x0, sizeOfVector(debugTraces));
                debugTraceBuffer.read(debugTraces, 0, sizeOfVector(debugTraces));
                memset(debugRays.data(), 0x0, sizeOfVector(debugRays));
                debugRayBuffer.read(debugRays, 0, sizeOfVector(debugRays));
        #if 0
                for (i32 i = 0; i < kNumDebugIterations; ++i)
                {
                    printf("===== iteration %d ===== \n", debugTraces[i].iteration);
                    printf("stepDepth: %.5f, minDepth: %.5f \n", debugTraces[i].stepDepth, debugTraces[i].minDepth);
                    printf("tDepth: %.5f, tCellBoundry: %.5f \n", debugTraces[i].tDepth, debugTraces[i].tCellBoundry);
                    printf("t: %.5f \n", debugTraces[i].t);
                    printf("level: %d ", debugTraces[i].level);
                    printf("mipSize: %dx%d \n", (i32)debugTraces[i].mipSize.x, (i32)debugTraces[i].mipSize.y);
                    printf("pp: (%.5f, %.5f) \n", debugTraces[i].pp.x, debugTraces[i].pp.y);
                    printf("cell: (%.5f, %.5f) ", debugTraces[i].cell.x, debugTraces[i].cell.y);
                    printf("cellBoundry: (l %.5f, r %.5f, b %.5f, t %.5f) \n", debugTraces[i].cellBoundry.x, debugTraces[i].cellBoundry.y, debugTraces[i].cellBoundry.z, debugTraces[i].cellBoundry.w);
                    printf("stepCount: %d \n", debugTraces[i].stepCount);
                }
        #endif
            }
        }
        // draw visualizations
        {
            // m_sceneTextures.renderTarget->setColorBuffer(m_sceneTextures.color, 0);
            // m_sceneTextures.renderTarget->setDrawBuffers({ 0 });
            for (i32 ray = 0; ray < numDebugRays; ++ray)
            {
                std::vector<Vertex> vertices;
                for (i32 i = 0; i < kNumIterations; ++i)
                {
                    i32 index = ray * kNumIterations + i;
                    if (debugRays[index].position.w > 0.f)
                    {
                        vertices.push_back(debugRays[index]);
                    }
                }
                // visualize the debug ray
                {
                    drawWorldSpaceLines(m_sceneTextures->renderTarget, vertices);
                    drawWorldSpacePoints(m_sceneTextures->renderTarget, vertices);
                }
            }
#if 0
            // todo: visualize the hierarchical trace 
            {
                for (i32 i = 1; i < kNumIterations; ++i)
                {
                    // catch the moment when the ray marched a step
                    if (debugTraceBuffer[i].stepCount > debugTraceBuffer[i - 1].stepCount)
                    {
                        i32 level = debugTraceBuffer[i - 1].level;
                        glm::vec4 cellBoundry = debugTraceBuffer[i - 1].cellBoundry * 2.f - 1.f;
                        /*
                          (l, t) ______  (r, t)
                            |               |
                            |               |
                            |               |
                            |               |
                          (l, b) ______  (r, b)
                        */ 
                        f32 l = cellBoundry.x, r = cellBoundry.y, b = cellBoundry.z, t = cellBoundry.w;
                        glm::vec4 color = glm::vec4(1.f, 1.f, 0.f, 1.f);
                        std::vector<Vertex> vertices = {
                            Vertex{ glm::vec4(l, t, 0.f, 1.f), color },
                            Vertex{ glm::vec4(r, t, 0.f, 1.f), color },
                            Vertex{ glm::vec4(r, b, 0.f, 1.f), color },
                            Vertex{ glm::vec4(l, b, 0.f, 1.f), color },
                            Vertex{ glm::vec4(l, t, 0.f, 1.f), color }
                        };
                        drawScreenSpaceLines(m_sceneTextures.renderTarget, vertices);
                    }
                }
            }
#endif
            // draw a full screen quad by treating every pixel as a perfect mirror
            {
                CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
                CreatePS(ps, "SSRTDebugMirrorPS", SHADER_SOURCE_PATH "ssrt_debug_mirror_p.glsl");
                CreatePixelPipeline(pipeline, "SSRTDebugMirror", vs, ps);
                auto renderTarget = createCachedRenderTarget("SSRTDebugMirror", m_sceneTextures->renderTarget->width, m_sceneTextures->renderTarget->height);
                renderTarget->setColorBuffer(m_sceneTextures->ssgiMirror.getGfxTexture2D(), 0);
                renderTarget->setDrawBuffers({ 0 });
                renderTarget->clearDrawBuffer({ 0 }, glm::vec4(0.f, 0.f, 0.f, 1.f), false);
                drawFullscreenQuad(
                    renderTarget,
                    pipeline,
                    [this](VertexShader* vs, PixelShader* ps) {
                        ps->setTexture("sceneDepth", m_sceneTextures->gBuffer.depth.getGfxTexture2D());
                        ps->setTexture("sceneNormal", m_sceneTextures->gBuffer.normal.getGfxTexture2D());
                        ps->setTexture("directDiffuseBuffer", m_sceneTextures->gBuffer.normal.getGfxTexture2D());
                        ps->setUniform("numLevels", (i32)m_sceneTextures->HiZ.texture.getGfxTexture2D());
                        ps->setTexture("HiZ", m_sceneTextures->HiZ.texture.getGfxTexture2D());
                        ps->setUniform("kMaxNumIterations", kNumIterations);
                    }
                );
            }
        }
    }

    // todo: try to defer drawing debug objects till after the post-processing pass
    void Renderer::drawWorldSpacePoints(RenderTarget* renderTarget, const std::vector<Vertex>& points)
    {
#if 0
        static ShaderStorageBuffer<DynamicSsboData<Vertex>> vertexBuffer("VertexBuffer", 1024);

        debugDrawCalls.push([this, renderTarget, points]() {
                // setup buffer
                assert(points.size() < vertexBuffer.getNumElements());
                u32 numPoints = points.size();
                // this maybe unsafe
                memcpy(vertexBuffer.data.array.data(), points.data(), sizeof(Vertex) * points.size());
                vertexBuffer.upload();

                // draw
                CreateVS(vs, "DebugDrawPointVS", SHADER_SOURCE_PATH "debug_draw_point_v.glsl");
                CreatePS(ps, "DebugDrawLinePS", SHADER_SOURCE_PATH "debug_draw_line_worldspace_p.glsl");
                CreatePixelPipeline(pipeline, "DebugDrawPoint", vs, ps);
                m_ctx->setDepthControl(DepthControl::kEnable);
                m_ctx->setShaderStorageBuffer(&vertexBuffer);
                m_ctx->setPixelPipeline(pipeline);
                m_ctx->setRenderTarget(renderTarget);
                m_ctx->setViewport({ 0, 0, renderTarget->width, renderTarget->height });
                glDrawArrays(GL_POINTS, 0, numPoints);
            }
        );
#endif
    }

    void Renderer::drawWorldSpaceLines(RenderTarget* renderTarget, const std::vector<Vertex>& vertices)
    {
        const u32 kMaxNumVertices = 1024;
        static ShaderStorageBuffer vertexBuffer("VertexBuffer", sizeof(Vertex) * kMaxNumVertices);

#if 0
        debugDrawCalls.push([this, renderTarget, vertices]() {
                // setup buffer
                u32 numVertices = vertices.size();
                u32 numLineSegments = max(i32(numVertices - 1), (i32)0);
                u32 numVerticesToDraw = numLineSegments * 2;
                assert(vertices.size() < kMaxNumVertices);
                // this maybe unsafe
                memcpy(vertexBuffer.data.array.data(), vertices.data(), sizeof(Vertex) * vertices.size());
                vertexBuffer.upload();

                // draw
                CreateVS(vs, "DebugDrawWorldSpaceLineVS", SHADER_SOURCE_PATH "debug_draw_line_worldspace_v.glsl");
                CreatePS(ps, "DebugDrawWorldSpaceLinePS", SHADER_SOURCE_PATH "debug_draw_line_worldspace_p.glsl");
                CreatePixelPipeline(pipeline, "DebugDrawLineWorldSpace", vs, ps);
                m_ctx->setDepthControl(DepthControl::kEnable);
                m_ctx->setShaderStorageBuffer(&vertexBuffer);
                m_ctx->setPixelPipeline(pipeline);
                m_ctx->setRenderTarget(renderTarget);
                m_ctx->setViewport({ 0, 0, renderTarget->width, renderTarget->height });
                glDrawArrays(GL_LINES, 0, numVerticesToDraw);
            }
        );
#endif
    }

    void Renderer::drawScreenSpaceLines(RenderTarget* renderTarget, const std::vector<Vertex>& vertices)
    {
#if 0
        static ShaderStorageBuffer<DynamicSsboData<Vertex>> vertexBuffer("VertexBuffer", 1024);

        debugDrawCalls.push([this, renderTarget, vertices] {
                // setup buffer
                u32 numVertices = vertices.size();
                u32 numLineSegments = max(numVertices - 1, 0);
                u32 numVerticesToDraw = numLineSegments * 2;
                assert(vertices.size() < vertexBuffer.getNumElements());
                // this maybe unsafe
                memcpy(vertexBuffer.data.array.data(), vertices.data(), sizeof(Vertex) * vertices.size());
                vertexBuffer.upload();

                // draw
                CreateVS(vs, "DebugDrawScreenSpaceLineVS", SHADER_SOURCE_PATH "debug_draw_line_screenspace_v.glsl");
                CreatePS(ps, "DebugDrawWorldSpaceLinePS", SHADER_SOURCE_PATH "debug_draw_line_worldspace_p.glsl");
                CreatePixelPipeline(pipeline, "DebugDrawLineScreenSpace", vs, ps);
                m_ctx->setDepthControl(DepthControl::kDisable);
                m_ctx->setShaderStorageBuffer(&vertexBuffer);
                m_ctx->setPixelPipeline(pipeline);
                m_ctx->setRenderTarget(renderTarget);
                m_ctx->setViewport({ 0, 0, renderTarget->width, renderTarget->height });
                glDrawArrays(GL_LINES, 0, numVerticesToDraw);
            }
        );
#endif
    }

    void Renderer::drawDebugObjects()
    {
        while (!debugDrawCalls.empty())
        {
            auto& draw = debugDrawCalls.front();
            draw();
            debugDrawCalls.pop();
        }
    }

    void Renderer::renderSceneGBufferWithTextureAtlas(RenderTarget* outRenderTarget, RenderableScene& scene, GBuffer gBuffer)
    {
        CreateVS(vs, "SceneColorPassVS", SHADER_SOURCE_PATH "scene_pass_v.glsl");
        CreatePS(ps, "SceneGBufferWithTextureAtlasPassPS", SHADER_SOURCE_PATH "scene_gbuffer_texture_atlas_p.glsl");
        CreatePixelPipeline(pipeline, "SceneGBufferWithTextureAtlasPass", vs, ps);
        m_ctx->setPixelPipeline(pipeline, [](VertexShader* vs, PixelShader* ps) {
        });

        outRenderTarget->setColorBuffer(gBuffer.albedo.getGfxTexture2D(), 0);
        outRenderTarget->setColorBuffer(gBuffer.normal.getGfxTexture2D(), 1);
        outRenderTarget->setColorBuffer(gBuffer.metallicRoughness.getGfxTexture2D(), 2);
        outRenderTarget->setDrawBuffers({ 0, 1, 2 });
        outRenderTarget->clearDrawBuffer(0, glm::vec4(0.f, 0.f, 0.f, 1.f), false);
        outRenderTarget->clearDrawBuffer(1, glm::vec4(0.f, 0.f, 0.f, 1.f), false);
        outRenderTarget->clearDrawBuffer(2, glm::vec4(0.f, 0.f, 0.f, 1.f), false);

        m_ctx->setRenderTarget(outRenderTarget);
        m_ctx->setViewport({ 0, 0, outRenderTarget->width, outRenderTarget->height });
        m_ctx->setDepthControl(DepthControl::kEnable);
        scene.bind(m_ctx);
        m_ctx->multiDrawArrayIndirect(scene.indirectDrawBuffer.get());
    }

    void Renderer::downsample(GfxTexture2D* src, GfxTexture2D* dst) {
        auto renderTarget = createCachedRenderTarget("Downsample", dst->width, dst->height);
        renderTarget->setColorBuffer(dst, 0);
        CreateVS(vs, "DownsampleVS", SHADER_SOURCE_PATH "downsample_v.glsl");
        CreatePS(ps, "DownsamplePS", SHADER_SOURCE_PATH "downsample_p.glsl");
        CreatePixelPipeline(pipeline, "Downsample", vs, ps);
        drawFullscreenQuad(
            renderTarget,
            pipeline, 
            [src](VertexShader* vs, PixelShader* ps) {
                ps->setTexture("srcTexture", src);
            }
        );
    }

    void Renderer::upscale(GfxTexture2D* src, GfxTexture2D* dst)
    {
        auto renderTarget = createCachedRenderTarget("Upscale", dst->width, dst->height);
        renderTarget->setColorBuffer(dst, 0);
        CreateVS(vs, "UpscaleVS", SHADER_SOURCE_PATH "upscale_v.glsl");
        CreatePS(ps, "UpscalePS", SHADER_SOURCE_PATH "upscale_p.glsl");
        CreatePixelPipeline(pipeline, "Upscale", vs, ps);
        drawFullscreenQuad(
            renderTarget,
            pipeline, 
            [src](VertexShader* vs, PixelShader* ps) {
                ps->setTexture("srcTexture", src);
            }
        );
    }

    std::unordered_multimap<GfxTexture2D::Spec, GfxTexture2D*> RenderTexture2D::cache;

    RenderTexture2D Renderer::bloom(GfxTexture2D* src)
    {
        // setup pass
        RenderTexture2D bloomSetupTexture("BloomSetup", src->getSpec());
        GfxTexture2D* bloomSetupGfxTexture = bloomSetupTexture.getGfxTexture2D();
        auto renderTarget = createCachedRenderTarget("BloomSetup", bloomSetupGfxTexture->width, bloomSetupGfxTexture->height);
        renderTarget->setColorBuffer(bloomSetupGfxTexture, 0);
        CreateVS(vs, "BloomSetupVS", SHADER_SOURCE_PATH "bloom_setup_v.glsl");
        CreatePS(ps, "BloomSetupPS", SHADER_SOURCE_PATH "bloom_setup_p.glsl");
        CreatePixelPipeline(pipeline, "BloomSetup", vs, ps);
        drawFullscreenQuad(
            renderTarget,
            pipeline,
            [src](VertexShader* vs, PixelShader* ps) {
                ps->setTexture("srcTexture", src);
            }
        );

        const i32 numPasses = 5;
        RenderTexture2D downsamplePyramid[numPasses + 1] = { };
        // downsample passes
        {
            downsamplePyramid[0] = bloomSetupTexture;
            for (i32 pass = 1; pass <= numPasses; ++pass)
            { 
                GfxTexture2D* src = downsamplePyramid[pass - 1].getGfxTexture2D();
                std::string passName("BloomDownsample");
                passName += '[' + std::to_string(pass) + ']';
                GfxTexture2D::Spec spec = src->getSpec();
                spec.width /= 2;
                spec.height /= 2;
                if (spec.width == 0u || spec.height == 0u)
                {
                    assert(0);
                }
                // todo: need to decouple Sampler from texture!!!
                // todo: this is a bug here, I shouldn't use point sampling here
                downsamplePyramid[pass] = RenderTexture2D(passName.c_str(), spec);
                GfxTexture2D* dst = downsamplePyramid[pass].getGfxTexture2D();
                downsample(src, dst);
            }
        }

        auto upscaleAndBlend = [this](GfxTexture2D* src, GfxTexture2D* blend, GfxTexture2D* dst) {
            auto renderTarget = createCachedRenderTarget("BloomUpscale", dst->width, dst->height);
            renderTarget->setColorBuffer(dst, 0);
            CreateVS(vs, "BloomUpscaleVS", SHADER_SOURCE_PATH "bloom_upscale_v.glsl");
            CreatePS(ps, "BloomUpscalePS", SHADER_SOURCE_PATH "bloom_upscale_p.glsl");
            CreatePixelPipeline(pipeline, "BloomUpscale", vs, ps);
            drawFullscreenQuad(
                renderTarget,
                pipeline,
                [src, blend](VertexShader* vs, PixelShader* ps) {
                    ps->setTexture("srcTexture", src);
                    ps->setTexture("blendTexture", blend);
                }
            );
        };

        RenderTexture2D upscalePyramid[numPasses + 1] = { };
        upscalePyramid[numPasses] = downsamplePyramid[numPasses];
        // upscale passes
        {
            for (i32 pass = numPasses; pass >= 1; --pass)
            { 
                GfxTexture2D* src = upscalePyramid[pass].getGfxTexture2D();
                GfxTexture2D* blend = downsamplePyramid[pass - 1].getGfxTexture2D();
                std::string passName("BloomUpscale");
                passName += '[' + std::to_string(pass) + ']';
                GfxTexture2D::Spec spec = src->getSpec();
                spec.width *= 2;
                spec.height *= 2;
                // todo: need to decouple Sampler from texture!!!
                // todo: this is a bug here, I shouldn't use point sampling here
                upscalePyramid[pass - 1] = RenderTexture2D(passName.c_str(), spec);
                GfxTexture2D* dst = upscalePyramid[pass - 1].getGfxTexture2D();

                upscaleAndBlend(src, blend, dst);
                gaussianBlur(dst, pass * 2, 1.f);
            }
        }
        return upscalePyramid[0];
    }

    void Renderer::compose(GfxTexture2D* composited, GfxTexture2D* inSceneColor, GfxTexture2D* inBloomColor, const glm::uvec2& outputResolution) 
    {
        auto renderTarget = createCachedRenderTarget("PostProcessing", composited->width, composited->height);
        renderTarget->setColorBuffer(composited, 0);

        CreateVS(vs, "CompositeVS", SHADER_SOURCE_PATH "composite_v.glsl");
        CreatePS(ps, "CompositePS", SHADER_SOURCE_PATH "composite_p.glsl");
        CreatePixelPipeline(pipeline, "Composite", vs, ps);

        drawFullscreenQuad(
            renderTarget,
            pipeline,
            [this, inBloomColor, inSceneColor](VertexShader* vs, PixelShader* ps) {
                ps->setUniform("enableTonemapping", m_settings.enableTonemapping ? 1.f : 0.f);
                ps->setUniform("tonemapOperator", m_settings.tonemapOperator);
                ps->setUniform("whitePointLuminance", m_settings.whitePointLuminance);
                ps->setUniform("smoothstepWhitePoint", m_settings.smoothstepWhitePoint);
                if (inBloomColor && m_settings.enableBloom) {
                    ps->setUniform("enableBloom", 1.f);
                } else {
                    ps->setUniform("enableBloom", 0.f);
                }
                ps->setUniform("exposure", m_settings.exposure)
                    .setUniform("colorTempreture", m_settings.colorTempreture)
                    .setUniform("bloomIntensity", m_settings.bloomIntensity)
                    .setTexture("bloomTexture", inBloomColor)
                    .setTexture("sceneColorTexture", inSceneColor);
            }
        );
        // add a reconstruction pass using Gaussian filter
        gaussianBlur(composited, 3u, 1.0f);
    }

    void Renderer::gaussianBlur(GfxTexture2D* inoutTexture, u32 inRadius, f32 inSigma) 
    {
        static const u32 kMaxkernelRadius = 10;
        static const u32 kMinKernelRadius = 2;

        struct GaussianKernel
        {
            GaussianKernel(u32 inRadius, f32 inSigma, LinearAllocator& allocator)
                : radius(Max(kMinKernelRadius, Min(inRadius, kMaxkernelRadius))), sigma(inSigma)
            {
                u32 kernelSize = radius;
                // todo: allocate weights from stack or frame allocator instead
                weights = new f32[kMaxkernelRadius];

                // calculate kernel weights
                f32 totalWeight = 0.f;
                const f32 step = 1.f;
                for (u32 i = 0; i < kernelSize; ++i)
                {
                    f32 x = (f32)i * step;
                    weights[i] = calcWeight(x);
                    totalWeight += weights[i];
                }

                // tweak the weights so that the boundry sample weight goes to 0
                f32 correction = calcWeight(step * kernelSize);
                for (u32 i = 0; i < kernelSize; ++i)
                {
                    weights[i] -= correction;
                    totalWeight -= correction;
                }

                // normalize the weights to 1
                totalWeight = totalWeight * 2.f - weights[0];
                for (u32 i = 0; i < kernelSize; ++i)
                {
                    weights[i] /= totalWeight;
                }
            };

            ~GaussianKernel()
            {
                delete[] weights;
            }

            f32 calcWeight(f32 x)
            {
                f32 coef = 1.f / (sigma * glm::sqrt(2.0f * M_PI));
                return coef * glm::exp(-.5f  * pow(x / sigma, 2.0));
            }

            u32 radius = 0u;
            f32 mean = 0.f;
            f32 sigma = 0.5f;
            f32* weights = nullptr;
        };

        CreateVS(vs, "GaussianBlurVS", SHADER_SOURCE_PATH "gaussian_blur_v.glsl");
        CreatePS(ps, "GaussianBlurPS", SHADER_SOURCE_PATH "gaussian_blur_p.glsl");
        CreatePixelPipeline(pipeline, "GaussianBlur", vs, ps);

        auto renderTarget = createCachedRenderTarget("GaussianBlur", inoutTexture->width, inoutTexture->height);
        renderTarget->setColorBuffer(inoutTexture, 0);

        // create scratch buffer for storing intermediate output
        std::string name("GaussianBlurScratch");
        GfxTexture2D::Spec spec = inoutTexture->getSpec();
        name += '_' + std::to_string(spec.width) + 'x' + std::to_string(spec.height);
        RenderTexture2D scratchBuffer(name.c_str(), spec);
        auto scratch = scratchBuffer.getGfxTexture2D();
        renderTarget->setColorBuffer(scratch, 1);

        GaussianKernel kernel(inRadius, inSigma, m_frameAllocator);

        // horizontal pass
        renderTarget->setDrawBuffers({ 1 });
        drawFullscreenQuad(
            renderTarget,
            pipeline,
            [inoutTexture, &kernel](VertexShader* vs, PixelShader* ps) {
                ps->setTexture("srcTexture", inoutTexture);
                ps->setUniform("kernelRadius", kernel.radius);
                ps->setUniform("pass", 1.f);
                for (u32 i = 0; i < kMaxkernelRadius; ++i)
                {
                    char name[32] = { };
                    sprintf_s(name, "weights[%d]", i);
                    ps->setUniform(name, kernel.weights[i]);
                }
            }
        );

        // vertical pass
        renderTarget->setDrawBuffers({ 0 });
        drawFullscreenQuad(
            renderTarget,
            pipeline,
            [scratch, &kernel](VertexShader* vs, PixelShader* ps) {
                ps->setTexture("srcTexture", scratch);
                ps->setUniform("kernelRadius", kernel.radius);
                ps->setUniform("pass", 0.f);
                for (u32 i = 0; i < kMaxkernelRadius; ++i)
                {
                    char name[32] = { };
                    sprintf_s(name, "weights[%d]", i);
                    ps->setUniform(name, kernel.weights[i]);
                }
            }
        );
    }

    void Renderer::addUIRenderCommand(const std::function<void()>& UIRenderCommand) {
        m_UIRenderCommandQueue.push(UIRenderCommand);
    }

    void Renderer::renderUI() 
    {
        // set to default render target
        m_ctx->setRenderTarget(nullptr, { });

        // begin imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        while (!m_UIRenderCommandQueue.empty())
        {
            const auto& command = m_UIRenderCommandQueue.front();
            command();
            m_UIRenderCommandQueue.pop();
        }

        // end imgui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        ImGuiIO& io = ImGui::GetIO(); (void)io;
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
    }

    void Renderer::debugDrawSphere(RenderTarget* renderTarget, const Viewport& viewport, const glm::vec3& position, const glm::vec3& scale, const glm::mat4& view, const glm::mat4& projection) {
        CreateVS(vs, "DebugDrawVS", "debug_draw_v.glsl");
        CreatePS(ps, "DebugDrawPS", "debug_draw_p.glsl");
        CreatePixelPipeline(pipeline, "DebugDraw", vs, ps);
        drawMesh(
            renderTarget,
            viewport,
            AssetManager::getAsset<StaticMesh>("Sphere"),
            pipeline,
            [this, position, scale, view, projection](VertexShader* vs, PixelShader* ps) {
                glm::mat4 mvp(1.f);
                mvp = glm::translate(mvp, position);
                mvp = glm::scale(mvp, scale);
                mvp = projection * view * mvp;
                vs->setUniform("mvp", mvp);
                ps->setUniform("color", glm::vec3(1.f, 0.f, 0.f));
            }
        );
    }

    void Renderer::debugDrawCubeImmediate(RenderTarget* renderTarget, const Viewport& viewport, const glm::vec3& position, const glm::vec3& scale, const glm::mat4& view, const glm::mat4& projection) {
        CreateVS(vs, "DebugDrawImmediateVS", "debug_draw_immediate_v.glsl");
        CreatePS(ps, "DebugDrawImmediatePS", "debug_draw_immediate_p.glsl");
        CreatePixelPipeline(pipeline, "GaussianBlur", vs, ps);
        drawMesh(
            renderTarget,
            viewport,
            AssetManager::getAsset<StaticMesh>("UnitCubeMesh"),
            pipeline,
            [this, position, scale, view, projection](VertexShader* vs, PixelShader* ps) {
                glm::mat4 mvp(1.f);
                mvp = glm::translate(mvp, position);
                mvp = glm::scale(mvp, scale);
                mvp = projection * view * mvp;
                vs->setUniform("mvp", mvp);
                ps->setUniform("color", glm::vec3(1.f, 0.f, 0.f));
            }
        );
    }

    void Renderer::debugDrawCubeBatched(RenderTarget* renderTarget, const Viewport& viewport, const glm::vec3& position, const glm::vec3& scale, const glm::vec3& facingDir, const glm::vec4& albedo, const glm::mat4& view, const glm::mat4& projection) {

    }

    void Renderer::debugDrawLineImmediate(const glm::vec3& v0, const glm::vec3& v1) {

    }

    void Renderer::debugDrawCubemap(TextureCube* cubemap) {
        static PerspectiveCamera camera(
            glm::vec3(0.f, 1.f, 2.f),
            glm::vec3(0.f, 0.f, 0.f),
            glm::vec3(0.f, 1.f, 0.f),
            90.f,
            0.1f,
            16.f,
            16.f / 9.f
        );

        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(320, 180));
        GfxTexture2D::Spec spec(320, 180, 1, PF_RGB16F);
        static GfxTexture2D* outTexture = createRenderTexture("CubemapViewerTexture", spec, Sampler2D());
        renderTarget->setColorBuffer(outTexture, 0);
        renderTarget->clearDrawBuffer(0, glm::vec4(0.2, 0.2, 0.2, 1.f));
        
        CreateVS(vs, "DebugDrawCubemapVS", "debug_draw_cubemap_v.glsl");
        CreatePS(ps, "DebugDrawCubemapPS", "debug_draw_cubemap_p.glsl");
        CreatePixelPipeline(pipeline, "DebugDrawCubemap", vs, ps);

        glDisable(GL_CULL_FACE);
        drawMesh(
            renderTarget.get(),
            {0, 0, renderTarget->width, renderTarget->height },
            AssetManager::getAsset<StaticMesh>("UnitCubeMesh"),
            pipeline,
            [cubemap](VertexShader* vs, PixelShader* ps) {
                vs->setUniform("model", glm::mat4(1.f));
                vs->setUniform("view", camera.view());
                vs->setUniform("projection", camera.projection());
                ps->setTexture("cubemap", cubemap);
            }
        );
        glEnable(GL_CULL_FACE);

        // draw the output texture to the cubemap viewer
        addUIRenderCommand([]() {
            ImGui::Begin("Debug Viewer"); {

                ImGui::Image((ImTextureID)outTexture->getGpuResource(), ImVec2(320, 180), ImVec2(0, 1), ImVec2(1, 0));

                // todo: implement a proper mouse input mechanism for this viewer
                // todo: abstract this into a debug viewer widget or something
                ImVec2 rectMin = ImGui::GetWindowContentRegionMin();
                ImVec2 rectMax = ImGui::GetWindowContentRegionMax();
                if (ImGui::IsMouseHoveringRect(rectMin, rectMax)) {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                        glm::dvec2 mouseCursorChange = IOSystem::get()->getMouseCursorChange();
                        const f32 C = 0.02f;
                        f32 phi = mouseCursorChange.x * C;
                        f32 theta = mouseCursorChange.y * C;
                        /** note - @min:
                        * copied from Camera.cpp CameraEntity::orbit();
                        */
                        glm::vec3 p = camera.position - camera.lookAt;
                        glm::quat quat(cos(.5f * -phi), sin(.5f * -phi) * camera.worldUp);
                        quat = glm::rotate(quat, -theta, camera.right());
                        glm::mat4 model(1.f);
                        model = glm::translate(model, camera.lookAt);
                        glm::mat4 rot = glm::toMat4(quat);
                        glm::vec4 pPrime = rot * glm::vec4(p, 1.f);
                        camera.position = glm::vec3(pPrime.x, pPrime.y, pPrime.z) + camera.lookAt;
                    }
                }
            } ImGui::End();
        });
    }

    void Renderer::debugDrawCubemap(GLuint cubemap) 
    {
        static PerspectiveCamera camera(
            glm::vec3(0.f, 1.f, 2.f),
            glm::vec3(0.f, 0.f, 0.f),
            glm::vec3(0.f, 1.f, 0.f),
            90.f,
            0.1f,
            16.f,
            16.f / 9.f
        );

        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(320, 180));
        GfxTexture2D::Spec spec(320, 180, 1, PF_RGB16F);
        static GfxTexture2D* outTexture = createRenderTexture("CubemapViewerTexture", spec, Sampler2D());
        renderTarget->setColorBuffer(outTexture, 0);
        renderTarget->clearDrawBuffer(0, glm::vec4(0.2, 0.2, 0.2, 1.f));
        
        CreateVS(vs, "DebugDrawCubemapVS", "debug_draw_cubemap_v.glsl");
        CreatePS(ps, "DebugDrawCubemapPS", "debug_draw_cubemap_p.glsl");
        CreatePixelPipeline(pipeline, "DebugDrawCubemap", vs, ps);

        glDisable(GL_CULL_FACE);
        drawMesh(
            renderTarget.get(),
            {0, 0, renderTarget->width, renderTarget->height },
            AssetManager::getAsset<StaticMesh>("UnitCubeMesh"),
            pipeline,
            [cubemap](VertexShader* vs, PixelShader* ps) {
                vs->setUniform("model", glm::mat4(1.f));
                vs->setUniform("view", camera.view());
                vs->setUniform("projection", camera.projection());
                ps->setUniform("cubemap", 128);
                glBindTextureUnit(128, cubemap);
                // shader->setTexture("cubemap", cubemap);
            }
        );
        glEnable(GL_CULL_FACE);

        // draw the output texture to the cubemap viewer
        addUIRenderCommand([]() {
            ImGui::Begin("Debug Viewer"); {

                ImGui::Image((ImTextureID)outTexture->getGpuResource(), ImVec2(320, 180), ImVec2(0, 1), ImVec2(1, 0));

                // todo: implement a proper mouse input mechanism for this viewer
                // todo: abstract this into a debug viewer widget or something
                ImVec2 rectMin = ImGui::GetWindowContentRegionMin();
                ImVec2 rectMax = ImGui::GetWindowContentRegionMax();
                if (ImGui::IsMouseHoveringRect(rectMin, rectMax)) {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                        glm::dvec2 mouseCursorChange = IOSystem::get()->getMouseCursorChange();
                        const f32 C = 0.02f;
                        f32 phi = mouseCursorChange.x * C;
                        f32 theta = mouseCursorChange.y * C;
                        /** note - @min:
                        * copied from Camera.cpp CameraEntity::orbit();
                        */
                        glm::vec3 p = camera.position - camera.lookAt;
                        glm::quat quat(cos(.5f * -phi), sin(.5f * -phi) * camera.worldUp);
                        quat = glm::rotate(quat, -theta, camera.right());
                        glm::mat4 model(1.f);
                        model = glm::translate(model, camera.lookAt);
                        glm::mat4 rot = glm::toMat4(quat);
                        glm::vec4 pPrime = rot * glm::vec4(p, 1.f);
                        camera.position = glm::vec3(pPrime.x, pPrime.y, pPrime.z) + camera.lookAt;
                    }
                }
            } ImGui::End();
        });
    }
}