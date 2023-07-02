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
#include "SkyBox.h"
#include "LightComponents.h"
#include "InputSystem.h"
#include "RenderPass.h"

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
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Build HZB");

        auto renderer = Renderer::get();
        auto gfxTexture = texture.getGfxTexture2D();
        // copy pass
        {
            CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            CreatePS(ps, "HiZCopyPS", SHADER_SOURCE_PATH "hi_z_copy_p.glsl");
            CreatePixelPipeline(pipeline, "HiZCopy", vs, ps);
            renderer->drawFullscreenQuad(
                glm::uvec2(gfxTexture->width, gfxTexture->height),
                [gfxTexture](RenderPass& pass) {
                    pass.setRenderTarget(RenderTarget(gfxTexture, 0, glm::vec4(1.f)), 0);
                },
                pipeline,
                [srcDepthTexture](ProgramPipeline* p) {
                    p->setTexture("srcDepthTexture", srcDepthTexture);
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
                auto src = i - 1;
                auto dst = i;
                renderer->drawFullscreenQuad(
                    glm::uvec2(mipWidth, mipHeight),
                    [gfxTexture, dst](RenderPass& pass) {
                        pass.setRenderTarget(RenderTarget(gfxTexture, dst), 0);
                    },
                    pipeline,
                    [this, src, gfxTexture](ProgramPipeline* p) {
                        p->setUniform("srcMip", src);
                        p->setTexture("srcDepthTexture", gfxTexture);
                    }
                );
            }
        }

        glPopDebugGroup();
    }

    GBuffer::GBuffer(const glm::uvec2& resolution)
        : depth("SceneGBufferDepth", GfxDepthTexture2D::Spec(resolution.x, resolution.y, 1), Sampler2D())
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

    Renderer::Renderer(GfxContext* ctx, u32 windowWidth, u32 windowHeight)
        : m_ctx(ctx),
        m_windowSize(windowWidth, windowHeight),
        m_frameAllocator(1024 * 1024 * 32)
    {
        m_SSGIRenderer = std::make_unique<SSGIRenderer>();
    }

    void Renderer::initialize() 
    {
        m_quad = AssetManager::findAsset<StaticMesh>("FullScreenQuadMesh");
    }

    void Renderer::deinitialize() 
    {
        // imgui clean up
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void Renderer::drawStaticMesh(const glm::uvec2& framebufferSize, const std::function<void(RenderPass&)>& setupRenderTarget, const Viewport& viewport, StaticMesh* mesh, PixelPipeline* pipeline, const ShaderSetupFunc& setupShader, const GfxPipelineState& gfxPipelineState)
    {
        RenderPass pass(framebufferSize.x, framebufferSize.y);
        setupRenderTarget(pass);
        pass.viewport = viewport;
        pass.gfxPipelineState = gfxPipelineState;
        pass.drawLambda = [mesh, pipeline, &setupShader](GfxContext* ctx) {
            pipeline->bind(ctx);
            setupShader(pipeline);
            for (u32 i = 0; i < mesh->getNumSubmeshes(); ++i) 
            {
                auto sm = (*mesh)[i];
                ctx->setVertexArray(sm->va.get());
                ctx->drawIndex(sm->numIndices());
            }
            pipeline->unbind(ctx);
        };
        pass.render(m_ctx);
    }

    void Renderer::drawFullscreenQuad(const glm::uvec2& framebufferSize, const std::function<void(RenderPass&)>& renderTargetSetupLambda, PixelPipeline* pipeline, const ShaderSetupFunc& setupShader)
    {
        Viewport viewport = { 0, 0, framebufferSize.x, framebufferSize.y };
        GfxPipelineState gfxPipelineState;
        gfxPipelineState.depth = DepthControl::kDisable;
        drawStaticMesh(framebufferSize, renderTargetSetupLambda, {0, 0, framebufferSize.x, framebufferSize.y }, m_quad.get(), pipeline, setupShader, gfxPipelineState);
    }

    void Renderer::drawScreenQuad(const glm::uvec2& framebufferSize, const std::function<void(RenderPass&)>& renderTargetSetupLambda, const Viewport& viewport, PixelPipeline* pipeline, const ShaderSetupFunc& setupShader)
    {
        GfxPipelineState gfxPipelineState;
        gfxPipelineState.depth = DepthControl::kDisable;
        drawStaticMesh(framebufferSize, renderTargetSetupLambda, viewport, m_quad.get(), pipeline, setupShader, gfxPipelineState);
    }

    void Renderer::drawColoredScreenSpaceQuad(GfxTexture2D* outTexture, const glm::vec2& screenSpaceMin, const glm::vec2& screenSpaceMax, const glm::vec4& color)
    {
        GfxPipelineState gfxPipelineState;
        gfxPipelineState.depth = DepthControl::kDisable;

        // calc viewport based on quad min and max in screen space
        Viewport viewport = { };
        viewport.x = screenSpaceMin.x * outTexture->width;
        viewport.y = screenSpaceMin.y * outTexture->height;
        viewport.width = (screenSpaceMax.x - screenSpaceMin.x) * outTexture->width;
        viewport.height = (screenSpaceMax.y - screenSpaceMin.y) * outTexture->height;

        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "ColoredQuadPS", SHADER_SOURCE_PATH "colored_quad_p.glsl");
        CreatePixelPipeline(pipeline, "ColoredQuad", vs, ps);

        drawStaticMesh(
            glm::uvec2(outTexture->width, outTexture->height), 
            [outTexture](RenderPass& pass) {
                pass.setRenderTarget(outTexture, 0);
            },
            viewport,
            m_quad.get(),
            pipeline,
            [color](ProgramPipeline* p) {
                p->setUniform("color", color);
            },
            gfxPipelineState
        );
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

    void Renderer::buildCubemapFromHDRI(GfxTextureCube* outCubemap, GfxTexture2D* HDRI)
    {
        std::string cubemapName = std::string("TextureCube ") + std::to_string(outCubemap->getGpuResource());
        std::string HDRIName = std::string("Texture2D " ) + std::to_string(HDRI->getGpuResource());
        std::string markerName = std::string("BuildCubemapFromHDRI (") + HDRIName + std::string("->") + cubemapName + std::string(")");
        GPU_DEBUG_SCOPE(buildCubemapFromHDRIMarker, markerName.c_str());

        CreateVS(vs, "BuildCubemapVS", SHADER_SOURCE_PATH "build_cubemap_v.glsl");
        CreatePS(ps, "RenderToCubemapPS", SHADER_SOURCE_PATH "render_to_cubemap_p.glsl");
        CreatePixelPipeline(pipeline, "RenderToCubemap", vs, ps);
        StaticMesh* cubeMesh = AssetManager::findAsset<StaticMesh>("UnitCubeMesh").get();

        const i32 kNumCubeFaces = 6u;
        for (i32 f = 0; f < kNumCubeFaces; f++)
        {
            GfxPipelineState gfxPipelineState;
            gfxPipelineState.depth = DepthControl::kDisable;

            Renderer::get()->drawStaticMesh(
                getFramebufferSize(outCubemap),
                [f, outCubemap](RenderPass& pass) {
                    pass.setRenderTarget(RenderTarget(outCubemap, f), 0);
                },
                { 0, 0, outCubemap->resolution, outCubemap->resolution },
                cubeMesh,
                pipeline,
                [f, HDRI](ProgramPipeline* p) {
                    Camera camera;
                    camera.m_worldSpacePosition = glm::vec3(0.f);
                    camera.m_worldUp = worldUps[f];
                    camera.m_worldSpaceForward = cameraFacingDirections[f];
                    camera.m_perspective.n = .1f;
                    camera.m_perspective.f = 100.f;
                    camera.m_perspective.fov = 90.f;
                    camera.m_perspective.aspectRatio = 1.f;
                    p->setUniform("cameraView", camera.viewMatrix());
                    p->setUniform("cameraProjection", camera.projectionMatrix());
                    p->setTexture("srcHDRI", HDRI);
                },
                gfxPipelineState
            );
        }

        // generate mipmap
        glGenerateTextureMipmap(outCubemap->getGpuResource());
    }

    void Renderer::renderSceneDepth(GfxDepthTexture2D* outDepthBuffer, Scene* scene, const SceneCamera::ViewParameters& viewParameters)
    {
        if (outDepthBuffer != nullptr)
        {
            CreateVS(vs, "StaticMeshVS", SHADER_SOURCE_PATH "static_mesh_v.glsl");
            CreatePS(ps, "DepthOnlyPS", SHADER_SOURCE_PATH "depth_only_p.glsl");
            CreatePixelPipeline(pipeline, "SceneDepthPrepass", vs, ps);

            RenderPass pass(outDepthBuffer->width, outDepthBuffer->height);
            pass.viewport = { 0, 0, outDepthBuffer->width, outDepthBuffer->height };
            pass.setDepthBuffer(outDepthBuffer);
            pass.drawLambda = [&scene, pipeline, viewParameters](GfxContext* ctx) { 
                pipeline->bind(ctx);
                viewParameters.setShaderParameters(pipeline);
                for (auto instance : scene->m_staticMeshInstances)
                {
                    pipeline->setUniform("localToWorld", instance->localToWorld.toMatrix());
                    auto mesh = instance->parent;
                    for (i32 i = 0; i < mesh->getNumSubmeshes(); ++i)
                    {
                        auto sm = (*mesh)[i];
                        ctx->setVertexArray(sm->va.get());
                        ctx->drawIndex(sm->numIndices());
                    }
                }
                pipeline->unbind(ctx);
            };
            pass.render(m_ctx);
        }
    }

    void Renderer::renderSceneDepthPrepass(GfxDepthTexture2D* outDepthBuffer, Scene* scene, const SceneCamera::ViewParameters& viewParameters)
    {
        GPU_DEBUG_SCOPE(sceneDepthPrepassMarker, "Scene Depth Prepass");

        if (outDepthBuffer != nullptr)
        {
            CreateVS(vs, "StaticMeshVS", SHADER_SOURCE_PATH "static_mesh_v.glsl");
            CreatePS(ps, "DepthOnlyPS", SHADER_SOURCE_PATH "depth_only_p.glsl");
            CreatePixelPipeline(pipeline, "SceneDepthPrepass", vs, ps);

            RenderPass pass(outDepthBuffer->width, outDepthBuffer->height);
            pass.viewport = { 0, 0, outDepthBuffer->width, outDepthBuffer->height };
            pass.setDepthBuffer(outDepthBuffer);
            pass.drawLambda = [scene, pipeline, viewParameters](GfxContext* ctx) { 
                pipeline->bind(ctx);
                viewParameters.setShaderParameters(pipeline);
                for (auto instance : scene->m_staticMeshInstances)
                {
                    pipeline->setUniform("localToWorld", instance->localToWorld.toMatrix());
                    auto mesh = instance->parent;
                    for (i32 i = 0; i < mesh->getNumSubmeshes(); ++i)
                    {
                        auto sm = (*mesh)[i];
                        ctx->setVertexArray(sm->va.get());
                        ctx->drawIndex(sm->numIndices());
                    }
                }
                pipeline->unbind(ctx);
            };
            pass.render(m_ctx);
        }
    }

    // todo: continue implementing this later
    void Renderer::renderVirtualShadowMap(Scene* scene, SceneRender* render)
    {
        // build page request for each pixel
    }

    /**
     * todo: implement material bucketing
     * todo: implement material->unbind()
     */
    void Renderer::renderSceneGBuffer(GfxTexture2D* outAlbedo, GfxTexture2D* outNormal, GfxTexture2D* outMetallicRoughness, GfxDepthTexture2D* depth, Scene* scene, const SceneCamera::ViewParameters& viewParameters)
    {
        GPU_DEBUG_SCOPE(renderSceneGBufferPassMarker, "Scene GBuffer Pass");

        if (outAlbedo != nullptr && outNormal != nullptr && outMetallicRoughness != nullptr && depth != nullptr && scene != nullptr)
        {
            RenderPass pass(outAlbedo->width, outAlbedo->height);
            pass.viewport = { 0, 0, outAlbedo->width, outAlbedo->height };
            pass.setRenderTarget(outAlbedo, 0);
            pass.setRenderTarget(outNormal, 1);
            pass.setRenderTarget(outMetallicRoughness, 2);
            pass.setDepthBuffer(depth, /*bClearDepth=*/false);
            pass.drawLambda = [&scene, viewParameters](GfxContext* ctx) { 
                for (auto instance : scene->m_staticMeshInstances)
                {
                    auto mesh = instance->parent;
                    for (i32 i = 0; i < mesh->getNumSubmeshes(); ++i)
                    {
                        auto sm = (*mesh)[i];
                        auto material = (*instance)[i];
                        material->bind(ctx, instance->localToWorld.toMatrix(), viewParameters);
                        ctx->setVertexArray(sm->va.get());
                        ctx->drawIndex(sm->numIndices());
                    }
                }
            };
            pass.render(m_ctx);
        }
    }

    void Renderer::renderSceneLighting(Scene* scene, SceneRender* render, const Camera& camera, const SceneCamera::ViewParameters& viewParameters)
    {
        GPU_DEBUG_SCOPE(sceneGBufferPassMarker, "Scene Lighting Pass");

        renderSceneDirectLighting(scene, render, camera, viewParameters);
        renderSceneIndirectLighting(scene, render, viewParameters);

        // compose lighting results
        auto outSceneColor = render->color();

        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "SceneLightingPassPS", SHADER_SOURCE_PATH "scene_lighting_p.glsl");
        CreatePixelPipeline(pipeline, "SceneLightingPass", vs, ps);

        drawFullscreenQuad(
            glm::uvec2(outSceneColor->width, outSceneColor->height),
            [outSceneColor](RenderPass& pass) {
                pass.setRenderTarget(outSceneColor, 0);
            },
            pipeline,
            [this, render](ProgramPipeline* p) {
                p->setTexture("directLightingTexture", render->directLighting());
                p->setTexture("indirectLightingTexture", render->indirectLighting());
            }
        );
    }

    void Renderer::renderSceneDirectLighting(Scene* scene, SceneRender* render, const Camera& camera, const SceneCamera::ViewParameters& viewParameters)
    {
        GPU_DEBUG_SCOPE(sceneDirectLightingPassMarker, "Scene Direct Lighting Pass");

        // render directional shadow map
        render->m_csm->render(scene, scene->m_directionalLight, camera);

        CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
        CreatePS(ps, "SceneDirectLightingPS", SHADER_SOURCE_PATH "scene_direct_lighting_p.glsl");
        CreatePixelPipeline(pipeline, "SceneDirectLighting", vs, ps);

        auto outDirectLighting = render->directLighting();
        auto outDirectDiffuseLighting = render->directDiffuseLighting();
        auto outLightingOnly = render->lightingOnly();

        drawFullscreenQuad(
            glm::uvec2(outDirectLighting->width, outDirectLighting->height),
            [outDirectLighting, outDirectDiffuseLighting, outLightingOnly, this](RenderPass& pass) {
                pass.setRenderTarget(outDirectLighting, 0);
                pass.setRenderTarget(outDirectDiffuseLighting, 1);
                pass.setRenderTarget(outLightingOnly, 2);
            },
            pipeline,
            [scene, render, viewParameters](ProgramPipeline* p) {
                viewParameters.setShaderParameters(p);
                // setup shader parameters for the directional light
                if (scene->m_directionalLight != nullptr)
                {
                    p->setUniform("directionalLight.color", scene->m_directionalLight->m_color);
                    p->setUniform("directionalLight.intensity", scene->m_directionalLight->m_intensity);
                    p->setUniform("directionalLight.direction", scene->m_directionalLight->m_direction);
                    auto csm = render->m_csm.get();
                    csm->setShaderParameters(p);
                }
                p->setTexture("sceneDepth", render->depth());
                p->setTexture("sceneNormal", render->normal());
                p->setTexture("sceneAlbedo", render->albedo());
                p->setTexture("sceneMetallicRoughness", render->metallicRoughness());
            }
        );
    }

    void Renderer::renderSceneIndirectLighting(Scene* scene, SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
        GPU_DEBUG_SCOPE(sceneIndirectLighting, "SceneIndirectLighting");

        auto outIndirectLighting = render->indirectLighting();
        auto depth = render->depth();

        // render skybox pass
        {
            GPU_DEBUG_SCOPE(skybox, "Skybox");

            CreateVS(vs, "SkyboxVS", SHADER_SOURCE_PATH "skybox_v.glsl");
            CreatePS(ps, "SkyboxPS", SHADER_SOURCE_PATH "skybox_p.glsl");
            CreatePixelPipeline(p, "Skybox", vs, ps);

            RenderPass pass(outIndirectLighting->width, outIndirectLighting->height);
            pass.viewport = { 0, 0, outIndirectLighting->width, outIndirectLighting->height };
            pass.setRenderTarget(outIndirectLighting, 0);
            pass.setDepthBuffer(depth, /*bClearDepth=*/false);
            pass.drawLambda = [p, scene, render, viewParameters](GfxContext* ctx) {
                if (scene->m_skybox != nullptr)
                {
                    p->bind(ctx);
                    p->setUniform("cameraView", viewParameters.viewMatrix);
                    p->setUniform("cameraProjection", viewParameters.projectionMatrix);
                    p->setTexture("cubemap", scene->m_skybox->m_cubemap.get());

                    auto cubeMesh = AssetManager::findAsset<StaticMesh>("UnitCubeMesh");
                    auto sm = (*cubeMesh)[0];
                    ctx->setVertexArray(sm->va.get());
                    ctx->drawIndex(sm->numIndices());
                    p->unbind(ctx);
                }
            };

            pass.render(m_ctx);
        }

        // render ao and indirect irradiance
        {
            m_SSGIRenderer->renderAO(render, viewParameters);
            m_SSGIRenderer->renderDiffuse(render, viewParameters);
        }

        // render indirect lighting effects pass
        {
            GPU_DEBUG_SCOPE(indirectLighting, "IndirectLighting");

            CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            CreatePS(ps, "SceneIndirectLightingPass", SHADER_SOURCE_PATH "scene_indirect_lighting_p.glsl");
            CreatePixelPipeline(pipeline, "SceneIndirectLightingPass", vs, ps);

            drawFullscreenQuad(
                getFramebufferSize(outIndirectLighting),
                [outIndirectLighting](RenderPass& pass) {
                    pass.setRenderTarget(outIndirectLighting, 0, /*bClear=*/false);
                },
                pipeline,
                [this, scene, render, viewParameters](ProgramPipeline* p) {
                    viewParameters.setShaderParameters(p);
                    p->setTexture("sceneDepth", render->depth());
                    p->setTexture("sceneNormal", render->normal());
                    p->setTexture("sceneAlbedo", render->albedo());
                    p->setTexture("sceneMetallicRoughness", render->metallicRoughness());

                    // todo: apply lighting effects
                    // ssao (screenspace ambient occlusion)
                    p->setUniform("SSGIAOEnabled", m_settings.bSSAOEnabled ? 1.f : 0.f);
                    p->setTexture("SSGIAO", render->ao());
                    // ssgi (screenspace indirect irradiance)
                    p->setUniform("SSGIDiffuseEnabled", m_settings.bIndirectIrradianceEnabled ? 1.f : 0.f);
                    p->setTexture("SSGIDiffuse", render->indirectIrradiance());

                    // ssr (screenspace reflection)

                    // sky light
                    if (scene->m_skyLight != nullptr)
                    {
                        auto skyLight = scene->m_skyLight;
                        p->setTexture("skyLight.irradiance", skyLight->m_irradianceProbe->m_irradianceCubemap.get());
                        p->setTexture("skyLight.reflection", skyLight->m_reflectionProbe->m_reflectionCubemap.get());
                        p->setTexture("skyLight.BRDFLookupTexture", ReflectionProbe::s_BRDFLookupTexture.get());
                    }
                }
            );
        }
    }

    void Renderer::postprocess(GfxTexture2D* outResolvedColor, GfxTexture2D* color)
    {
        GPU_DEBUG_SCOPE(postprocessing, "PostProcessing");

        auto bloomTexture = bloom(color);
        compose(outResolvedColor, color, bloomTexture.getGfxTexture2D());
    }

    void Renderer::visualize(GfxTexture2D* dst, GfxTexture2D* src, i32 mip) 
    {
        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "BlitPS", SHADER_SOURCE_PATH "blit_p.glsl");
        CreatePixelPipeline(pipeline, "BlitQuad", vs, ps);

        drawFullscreenQuad(
            glm::uvec2(dst->width, dst->height),
            [dst, mip](RenderPass& pass) {
                pass.setRenderTarget(RenderTarget(dst, mip), 0);
            },
            pipeline,
            [this, src, mip](ProgramPipeline* p) {
                p->setTexture("srcTexture", src);
                p->setUniform("mip", mip);
            });
    }

    void Renderer::blitTexture(GfxTexture2D* dst, GfxTexture2D* src)
    {
        assert(dst->width == src->width && dst->height == src->height);

        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "BlitPS", SHADER_SOURCE_PATH "blit_p.glsl");
        CreatePixelPipeline(pipeline, "BlitQuad", vs, ps);

        // final blit to default framebuffer
        drawFullscreenQuad(
            glm::uvec2(dst->width, dst->height),
            [dst](RenderPass& pass) {
                pass.setRenderTarget(dst, 0);
            },
            pipeline,
            [this, src](ProgramPipeline* p) {
                p->setTexture("srcTexture", src);
                p->setUniform("mip", (i32)0);
            }
        );
    }

    void Renderer::copyDepth(GfxDepthTexture2D* dst, GfxDepthTexture2D* src)
    {
        assert(dst->width == src->width && dst->height == src->height);

        CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
        CreatePS(ps, "CopyDepthPS", SHADER_SOURCE_PATH "copy_depth_p.glsl");
        CreatePixelPipeline(pipeline, "CopyDepth", vs, ps);

        GfxPipelineState gfxPipelineState;
        gfxPipelineState.depth = DepthControl::kEnable;
        drawStaticMesh(
            getFramebufferSize(dst), 
            [dst](RenderPass& pass) {
                pass.setDepthBuffer(dst);
            },
            {0, 0, dst->width, dst->height}, 
            m_quad.get(), 
            pipeline, 
            [this, src](ProgramPipeline* p) {
                p->setTexture("srcDepth", src);
                p->setUniform("mip", (i32)0);
            },
            gfxPipelineState
        );
    }

    void Renderer::renderToScreen(GfxTexture2D* texture)
    {
        GPU_DEBUG_SCOPE(presentMarker, "Render to Screen");

        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "BlitPS", SHADER_SOURCE_PATH "blit_p.glsl");
        CreatePixelPipeline(pipeline, "BlitQuad", vs, ps);

        // final blit to default framebuffer
        drawFullscreenQuad(
            glm::uvec2(m_windowSize.x, m_windowSize.y),
            [](RenderPass& pass) {
            },
            pipeline,
            [this, texture](ProgramPipeline* p) {
                p->setTexture("srcTexture", texture);
                p->setUniform("mip", (i32)0);
            }
        );
    }

    void Renderer::endRender() 
    {
    }

#if 0
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
            cs->setShaderStorageBuffer(&debugTraceBuffer);
            cs->setShaderStorageBuffer(&debugRayBuffer);
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
#if 0
            // m_sceneTextures.framebuffer->setColorBuffer(m_sceneTextures.color, 0);
            // m_sceneTextures.framebuffer->setDrawBuffers({ 0 });
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
                    drawWorldSpaceLines(m_sceneTextures->framebuffer, vertices);
                    drawWorldSpacePoints(m_sceneTextures->framebuffer, vertices);
                }
            }
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
                        drawScreenSpaceLines(m_sceneTextures.framebuffer, vertices);
                    }
                }
            }
#endif
            // draw a full screen quad by treating every pixel as a perfect mirror
            {
#if 0 
                CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
                CreatePS(ps, "SSRTDebugMirrorPS", SHADER_SOURCE_PATH "ssgi_debug_mirror_p.glsl");
                CreatePixelPipeline(pipeline, "SSGIDebugMirror", vs, ps);
                auto framebuffer = createCachedFramebuffer("SSGIDebugMirror", m_sceneTextures->framebuffer->width, m_sceneTextures->framebuffer->height);
                framebuffer->setColorBuffer(m_sceneTextures->ssgiMirror.getGfxTexture2D(), 0);
                framebuffer->setDrawBuffers({ 0 });
                framebuffer->clearDrawBuffer({ 0 }, glm::vec4(0.f, 0.f, 0.f, 1.f), false);
                drawFullscreenQuad(
                    framebuffer,
                    pipeline,
                    [this](ProgramPipeline* p) {
                        p->setTexture("sceneDepth", m_sceneTextures->gBuffer.depth.getGfxDepthTexture2D());
                        p->setTexture("sceneNormal", m_sceneTextures->gBuffer.normal.getGfxTexture2D());
                        p->setTexture("directDiffuseBuffer", m_sceneTextures->gBuffer.normal.getGfxTexture2D());
                        p->setUniform("numLevels", (i32)m_sceneTextures->HiZ.texture.getGfxTexture2D());
                        p->setTexture("HiZ", m_sceneTextures->HiZ.texture.getGfxTexture2D());
                        p->setUniform("kMaxNumIterations", kNumIterations);
                    }
                );
#endif
            }
        }
    }
#endif

    void Renderer::debugDrawWorldSpacePoints(GfxTexture2D* outColor, GfxDepthTexture2D* depthBuffer, const SceneCamera::ViewParameters& viewParameters, const std::vector<DebugPrimitiveVertex>& points)
    {
        const u32 kMaxNumVertices = 1024;
        static ShaderStorageBuffer vertexBuffer("VertexBuffer", sizeof(DebugPrimitiveVertex) * kMaxNumVertices);
        assert(points.size() <= kMaxNumVertices);
        u32 numVertices = points.size();

        CreateVS(vs, "DebugDrawWorldSpacePointVS", SHADER_SOURCE_PATH "debug_draw_points_worldspace_v.glsl");
        CreatePS(ps, "DebugDrawPS", SHADER_SOURCE_PATH "debug_draw_p.glsl");
        CreatePixelPipeline(p, "DebugDrawPointWorldSpace", vs, ps);

        vertexBuffer.write(points, 0);

        RenderPass pass(outColor->width, outColor->height);
        pass.viewport = { 0, 0, outColor->width, outColor->height };
        pass.setRenderTarget(outColor, 0, /*bClear=*/false);
        pass.setDepthBuffer(depthBuffer, /*bClearDepth=*/false);
        pass.drawLambda = [p, viewParameters, numVertices](GfxContext* ctx) {
            p->bind(ctx);
            p->setShaderStorageBuffer(&vertexBuffer);
            auto va = VertexArray::getDummyVertexArray(); // work around not using a vertex array for drawing
            va->bind();
            p->setUniform("cameraView", viewParameters.viewMatrix);
            p->setUniform("cameraProjection", viewParameters.projectionMatrix);
            glDrawArrays(GL_POINTS, 0, numVertices);
            va->unbind();
            p->unbind(ctx);
        };

        pass.render(m_ctx);
    }

    void Renderer::debugDrawWorldSpaceLines(GfxTexture2D* outColor, GfxDepthTexture2D* depthBuffer, const SceneCamera::ViewParameters& viewParameters, const std::vector<DebugPrimitiveVertex>& vertices)
    {
        const u32 kMaxNumVertices = 1024;
        static ShaderStorageBuffer vertexBuffer("VertexBuffer", sizeof(DebugPrimitiveVertex) * kMaxNumVertices);
        assert(vertices.size() <= kMaxNumVertices);
        u32 numVertices = vertices.size();

        CreateVS(vs, "DebugDrawWorldSpaceVS", SHADER_SOURCE_PATH "debug_draw_worldspace_v.glsl");
        CreatePS(ps, "DebugDrawPS", SHADER_SOURCE_PATH "debug_draw_p.glsl");
        CreatePixelPipeline(p, "DebugDrawLineWorldSpace", vs, ps);

        vertexBuffer.write(vertices, 0);

        RenderPass pass(outColor->width, outColor->height);
        pass.viewport = { 0, 0, outColor->width, outColor->height };
        pass.setRenderTarget(outColor, 0, /*bClear=*/false);
        pass.setDepthBuffer(depthBuffer, /*bClearDepth=*/false);
        pass.drawLambda = [p, viewParameters, numVertices](GfxContext* ctx) {
            p->bind(ctx);
            p->setShaderStorageBuffer(&vertexBuffer);
            auto va = VertexArray::getDummyVertexArray(); // work around not using a vertex array for drawing
            va->bind();
            p->setUniform("cameraView", viewParameters.viewMatrix);
            p->setUniform("cameraProjection", viewParameters.projectionMatrix);
            glDrawArrays(GL_LINES, 0, numVertices);
            va->unbind();
            p->unbind(ctx);
        };

        pass.render(m_ctx);
    }

    void Renderer::debugDrawScreenSpaceLines(GfxTexture2D* outColor, GfxDepthTexture2D* depthBuffer, const std::vector<DebugPrimitiveVertex>& vertices)
    {
        const u32 kMaxNumVertices = 1024;
        static ShaderStorageBuffer vertexBuffer("VertexBuffer", sizeof(DebugPrimitiveVertex) * kMaxNumVertices);
        assert(vertices.size() <= kMaxNumVertices);
        u32 numVertices = vertices.size();

        CreateVS(vs, "DebugDrawScreenSpaceLineVS", SHADER_SOURCE_PATH "debug_draw_line_screenspace_v.glsl");
        CreatePS(ps, "DebugDrawPS", SHADER_SOURCE_PATH "debug_draw_p.glsl");
        CreatePixelPipeline(p, "DebugDrawLineScreenSpace", vs, ps);

        vertexBuffer.write(vertices, 0);

        RenderPass pass(outColor->width, outColor->height);
        pass.viewport = { 0, 0, outColor->width, outColor->height };
        pass.setRenderTarget(outColor, 0, /*bClear=*/false);
        pass.setDepthBuffer(depthBuffer, /*bClearDepth=*/false);
        pass.drawLambda = [p, numVertices](GfxContext* ctx) {
            p->bind(ctx);
            p->setShaderStorageBuffer(&vertexBuffer);
            auto va = VertexArray::getDummyVertexArray(); // work around not using a vertex array for drawing
            va->bind();
            glDrawArrays(GL_LINES, 0, numVertices);
            va->unbind();
            p->unbind(ctx);
        };

        pass.render(m_ctx);
    }

    void Renderer::debugDrawScreenSpacePoints(GfxTexture2D* outColor, GfxDepthTexture2D* depthBuffer, const SceneCamera::ViewParameters& viewParameters, const std::vector<DebugPrimitiveVertex>& vertices)
    {
        const u32 kMaxNumVertices = 1024;
        static ShaderStorageBuffer vertexBuffer("VertexBuffer", sizeof(DebugPrimitiveVertex) * kMaxNumVertices);
        assert(vertices.size() <= kMaxNumVertices);
        u32 numVertices = vertices.size();

        CreateVS(vs, "DebugDrawScreenSpacePointsVS", SHADER_SOURCE_PATH "debug_draw_points_screenspace_v.glsl");
        CreatePS(ps, "DebugDrawPS", SHADER_SOURCE_PATH "debug_draw_p.glsl");
        CreatePixelPipeline(p, "DebugDrawScreenSpacePoints", vs, ps);

        vertexBuffer.write(vertices, 0);

        RenderPass pass(outColor->width, outColor->height);
        pass.viewport = { 0, 0, outColor->width, outColor->height };
        pass.setRenderTarget(outColor, 0, /*bClear=*/false);
        pass.setDepthBuffer(depthBuffer, /*bClearDepth=*/false);
        pass.drawLambda = [p, viewParameters, numVertices](GfxContext* ctx) {
            p->bind(ctx);
            p->setShaderStorageBuffer(&vertexBuffer);
            auto va = VertexArray::getDummyVertexArray(); // work around not using a vertex array for drawing
            va->bind();
            p->setUniform("cameraView", viewParameters.viewMatrix);
            p->setUniform("cameraProjection", viewParameters.projectionMatrix);
            glDrawArrays(GL_POINTS, 0, numVertices);
            va->unbind();
            p->unbind(ctx);
        };
        GfxPipelineState gfxPipelineState = { };
        gfxPipelineState.depth = DepthControl::kDisable;
        pass.gfxPipelineState = gfxPipelineState;

        pass.render(m_ctx);
    }

#if 0
    void Renderer::renderSceneGBufferWithTextureAtlas(Framebuffer* outFramebuffer, RenderableScene& scene, GBuffer gBuffer)
    {
        CreateVS(vs, "SceneColorPassVS", SHADER_SOURCE_PATH "scene_pass_v.glsl");
        CreatePS(ps, "SceneGBufferWithTextureAtlasPassPS", SHADER_SOURCE_PATH "scene_gbuffer_texture_atlas_p.glsl");
        CreatePixelPipeline(pipeline, "SceneGBufferWithTextureAtlasPass", vs, ps);
        m_ctx->setPixelPipeline(pipeline, [](ProgramPipeline* p) {
        });

        outFramebuffer->setColorBuffer(gBuffer.albedo.getGfxTexture2D(), 0);
        outFramebuffer->setColorBuffer(gBuffer.normal.getGfxTexture2D(), 1);
        outFramebuffer->setColorBuffer(gBuffer.metallicRoughness.getGfxTexture2D(), 2);
        outFramebuffer->setDrawBuffers({ 0, 1, 2 });
        outFramebuffer->clearDrawBuffer(0, glm::vec4(0.f, 0.f, 0.f, 1.f), false);
        outFramebuffer->clearDrawBuffer(1, glm::vec4(0.f, 0.f, 0.f, 1.f), false);
        outFramebuffer->clearDrawBuffer(2, glm::vec4(0.f, 0.f, 0.f, 1.f), false);

        m_ctx->setFramebuffer(outFramebuffer);
        m_ctx->setViewport({ 0, 0, outFramebuffer->width, outFramebuffer->height });
        m_ctx->setDepthControl(DepthControl::kEnable);
        scene.bind(m_ctx);
        m_ctx->multiDrawArrayIndirect(scene.indirectDrawBuffer.get());
    }
#endif

    void Renderer::downsample(GfxTexture2D* src, GfxTexture2D* dst) 
    {
        CreateVS(vs, "DownsampleVS", SHADER_SOURCE_PATH "downsample_v.glsl");
        CreatePS(ps, "DownsamplePS", SHADER_SOURCE_PATH "downsample_p.glsl");
        CreatePixelPipeline(pipeline, "Downsample", vs, ps);

        drawFullscreenQuad(
            getFramebufferSize(dst),
            [dst](RenderPass& pass) {
                pass.setRenderTarget(dst, 0);
            },
            pipeline, 
            [src](ProgramPipeline* p) {
                p->setTexture("srcTexture", src);
            }
        );
    }

    void Renderer::upscale(GfxTexture2D* src, GfxTexture2D* dst)
    {
        CreateVS(vs, "UpscaleVS", SHADER_SOURCE_PATH "upscale_v.glsl");
        CreatePS(ps, "UpscalePS", SHADER_SOURCE_PATH "upscale_p.glsl");
        CreatePixelPipeline(pipeline, "Upscale", vs, ps);

        drawFullscreenQuad(
            getFramebufferSize(dst),
            [dst](RenderPass& pass) {
                pass.setRenderTarget(dst, 0);
            },
            pipeline, 
            [src](ProgramPipeline* p) {
                p->setTexture("srcTexture", src);
            }
        );
    }

    RenderTexture2D Renderer::bloom(GfxTexture2D* src)
    {
        GPU_DEBUG_SCOPE(bloomPassMarker, "Bloom");

        // setup pass
        RenderTexture2D bloomSetupTexture("BloomSetup", src->getSpec());
        GfxTexture2D* bloomSetupGfxTexture = bloomSetupTexture.getGfxTexture2D();

        CreateVS(vs, "BloomSetupVS", SHADER_SOURCE_PATH "bloom_setup_v.glsl");
        CreatePS(ps, "BloomSetupPS", SHADER_SOURCE_PATH "bloom_setup_p.glsl");
        CreatePixelPipeline(pipeline, "BloomSetup", vs, ps);

        drawFullscreenQuad(
            getFramebufferSize(bloomSetupGfxTexture),
            [bloomSetupGfxTexture](RenderPass& pass) {
                pass.setRenderTarget(bloomSetupGfxTexture, 0);
            },
            pipeline,
            [src](ProgramPipeline* p) {
                p->setTexture("srcTexture", src);
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
            CreateVS(vs, "BloomUpscaleVS", SHADER_SOURCE_PATH "bloom_upscale_v.glsl");
            CreatePS(ps, "BloomUpscalePS", SHADER_SOURCE_PATH "bloom_upscale_p.glsl");
            CreatePixelPipeline(pipeline, "BloomUpscale", vs, ps);

            drawFullscreenQuad(
                getFramebufferSize(dst),
                [dst](RenderPass& pass) {
                    pass.setRenderTarget(dst, 0);
                },
                pipeline,
                [src, blend](ProgramPipeline* p) {
                    p->setTexture("srcTexture", src);
                    p->setTexture("blendTexture", blend);
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

    void Renderer::compose(GfxTexture2D* outComposited, GfxTexture2D* sceneColor, GfxTexture2D* bloomColor) 
    {
        CreateVS(vs, "CompositeVS", SHADER_SOURCE_PATH "composite_v.glsl");
        CreatePS(ps, "CompositePS", SHADER_SOURCE_PATH "composite_p.glsl");
        CreatePixelPipeline(pipeline, "Composite", vs, ps);

        drawFullscreenQuad(
            getFramebufferSize(outComposited),
            [outComposited](RenderPass& pass) {
                pass.setRenderTarget(outComposited, 0);
            },
            pipeline,
            [this, bloomColor, sceneColor](ProgramPipeline* p) {
                p->setUniform("enableTonemapping", m_settings.enableTonemapping ? 1.f : 0.f);
                p->setUniform("tonemapOperator", m_settings.tonemapOperator);
                p->setUniform("whitePointLuminance", m_settings.whitePointLuminance);
                p->setUniform("smoothstepWhitePoint", m_settings.smoothstepWhitePoint);
                if (bloomColor && m_settings.enableBloom) 
                {
                    p->setUniform("enableBloom", 1.f);
                } 
                else 
                {
                    p->setUniform("enableBloom", 0.f);
                }
                p->setUniform("exposure", m_settings.exposure);
                p->setUniform("colorTempreture", m_settings.colorTempreture);
                p->setUniform("bloomIntensity", m_settings.bloomIntensity);
                p->setTexture("bloomTexture", bloomColor);
                p->setTexture("sceneColorTexture", sceneColor);
            }
        );
        // add a reconstruction pass using Gaussian filter
        gaussianBlur(outComposited, 3u, 1.0f);
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

        // create scratch buffer for storing intermediate output
        std::string m_name("GaussianBlurScratch");
        GfxTexture2D::Spec spec = inoutTexture->getSpec();
        m_name += '_' + std::to_string(spec.width) + 'x' + std::to_string(spec.height);
        RenderTexture2D scratchBuffer(m_name.c_str(), spec);
        auto scratch = scratchBuffer.getGfxTexture2D();

        GaussianKernel kernel(inRadius, inSigma, m_frameAllocator);

        // horizontal pass
        drawFullscreenQuad(
            getFramebufferSize(scratch),
            [scratch](RenderPass& pass) {
                pass.setRenderTarget(scratch, 0);
            },
            pipeline,
            [inoutTexture, &kernel](ProgramPipeline* p) {
                p->setTexture("srcTexture", inoutTexture);
                p->setUniform("kernelRadius", kernel.radius);
                p->setUniform("pass", 1.f);
                for (u32 i = 0; i < kMaxkernelRadius; ++i)
                {
                    char m_name[32] = { };
                    sprintf_s(m_name, "weights[%d]", i);
                    p->setUniform(m_name, kernel.weights[i]);
                }
            }
        );

        // vertical pass
        drawFullscreenQuad(
            getFramebufferSize(inoutTexture),
            [inoutTexture](RenderPass& pass) {
                pass.setRenderTarget(inoutTexture, 0);
            },
            pipeline,
            [scratch, &kernel](ProgramPipeline* p) {
                p->setTexture("srcTexture", scratch);
                p->setUniform("kernelRadius", kernel.radius);
                p->setUniform("pass", 0.f);
                for (u32 i = 0; i < kMaxkernelRadius; ++i)
                {
                    char m_name[32] = { };
                    sprintf_s(m_name, "weights[%d]", i);
                    p->setUniform(m_name, kernel.weights[i]);
                }
            }
        );
    }

    void Renderer::addUIRenderCommand(const std::function<void()>& UIRenderCommand) {
        m_UIRenderCommandQueue.push(UIRenderCommand);
    }

    void Renderer::renderUI() 
    {
        GPU_DEBUG_SCOPE(UIPassMarker, "Render UI");

        // set to default render target
        m_ctx->setFramebuffer(nullptr);

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


    void Renderer::debugDrawSphere(GfxTexture2D* outColor, GfxDepthTexture2D* depthBuffer, const glm::vec3& worldSpacePosition, f32 radius, const glm::vec3& color, const SceneCamera::ViewParameters& viewParameters)
    {
        CreateVS(vs, "DebugDrawVS", SHADER_SOURCE_PATH "debug_draw_v.glsl");
        CreatePS(ps, "DebugDrawPS", SHADER_SOURCE_PATH "debug_draw_p.glsl");
        CreatePixelPipeline(pipeline, "DebugDraw", vs, ps);

        auto sphere = AssetManager::findAsset<StaticMesh>("DefaultSphere");
        drawStaticMesh(
            glm::uvec2(outColor->width, outColor->height),
            [outColor, depthBuffer](RenderPass& pass) {
                pass.setRenderTarget(outColor, 0, /*bClear=*/false);
                pass.setDepthBuffer(depthBuffer, /*bClear=*/false);
            },
            { 0, 0, outColor->width, outColor->height },
            sphere.get(),
            pipeline,
            [radius, worldSpacePosition, color, viewParameters](ProgramPipeline* p) {
                glm::mat4 modelMatrix(1.f);
                modelMatrix = glm::translate(modelMatrix, worldSpacePosition);
                modelMatrix = glm::scale(modelMatrix, glm::vec3(radius));
                p->setUniform("color", color);
                p->setUniform("modelMatrix", modelMatrix);
                p->setUniform("cameraView", viewParameters.viewMatrix);
                p->setUniform("cameraProjection", viewParameters.projectionMatrix);
            },
            GfxPipelineState()
        );
    }

    void Renderer::debugDrawWireframeSphere(GfxTexture2D* outColor, GfxDepthTexture2D* depthBuffer, const glm::vec3& worldSpacePosition, f32 radius, const glm::vec3& color,  const SceneCamera::ViewParameters& viewParameters)
    {
        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        debugDrawSphere(outColor, depthBuffer, worldSpacePosition, radius, color, viewParameters);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_CULL_FACE);
    }

    void Renderer::debugDrawCube(GfxTexture2D* outColor, GfxDepthTexture2D* depthBuffer, const glm::vec3& worldSpacePosition, f32 radius, const glm::vec3& color, const SceneCamera::ViewParameters& viewParameters)
    {

    }

    void Renderer::debugDrawWireframeCube(GfxTexture2D* outColor, GfxDepthTexture2D* depthBuffer, const glm::vec3& worldSpacePosition, f32 radius, const glm::vec3& color, const SceneCamera::ViewParameters& viewParameters)
    {

    }
    void Renderer::debugDrawCubemap(GfxTextureCube* cubemap) {
#if 0
        static PerspectiveCamera camera(
            glm::vec3(0.f, 1.f, 2.f),
            glm::vec3(0.f, 0.f, 0.f),
            glm::vec3(0.f, 1.f, 0.f),
            90.f,
            0.1f,
            16.f,
            16.f / 9.f
        );

        auto framebuffer = createCachedFramebuffer("DebugDrawCubemapFramebuffer", 320, 180);
        GfxTexture2D::Spec spec(320, 180, 1, PF_RGB16F);
        static GfxTexture2D* outTexture = createRenderTexture("CubemapViewerTexture", spec, Sampler2D());
        framebuffer->setColorBuffer(outTexture, 0);
        framebuffer->clearDrawBuffer(0, glm::vec4(0.2, 0.2, 0.2, 1.f));
        
        CreateVS(vs, "DebugDrawCubemapVS", "debug_draw_cubemap_v.glsl");
        CreatePS(ps, "DebugDrawCubemapPS", "debug_draw_cubemap_p.glsl");
        CreatePixelPipeline(pipeline, "DebugDrawCubemap", vs, ps);

        glDisable(GL_CULL_FACE);
        drawStaticMesh(
            framebuffer,
            {0, 0, framebuffer->width, framebuffer->height },
            AssetManager::getAsset<StaticMesh>("UnitCubeMesh"),
            pipeline,
            [cubemap](ProgramPipeline* p) {
                p->setUniform("model", glm::mat4(1.f));
                p->setUniform("view", camera.view());
                p->setUniform("projection", camera.projection());
                p->setTexture("cubemap", cubemap);
            },
            GfxPipelineState { }
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
                        glm::dvec2 mouseCursorChange = InputSystem::get()->getMouseCursorChange();
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
#endif
    }

    void Renderer::debugDrawCubemap(GLuint cubemap) 
    {
#if 0
        static PerspectiveCamera camera(
            glm::vec3(0.f, 1.f, 2.f),
            glm::vec3(0.f, 0.f, 0.f),
            glm::vec3(0.f, 1.f, 0.f),
            90.f,
            0.1f,
            16.f,
            16.f / 9.f
        );
        auto framebuffer = createCachedFramebuffer("DebugDrawCubemapFramebuffer", 320, 180);
        GfxTexture2D::Spec spec(320, 180, 1, PF_RGB16F);
        static GfxTexture2D* outTexture = createRenderTexture("CubemapViewerTexture", spec, Sampler2D());
        framebuffer->setColorBuffer(outTexture, 0);
        framebuffer->clearDrawBuffer(0, glm::vec4(0.2, 0.2, 0.2, 1.f));
        
        CreateVS(vs, "DebugDrawCubemapVS", "debug_draw_cubemap_v.glsl");
        CreatePS(ps, "DebugDrawCubemapPS", "debug_draw_cubemap_p.glsl");
        CreatePixelPipeline(pipeline, "DebugDrawCubemap", vs, ps);

        glDisable(GL_CULL_FACE);
        drawStaticMesh(
            framebuffer,
            {0, 0, framebuffer->width, framebuffer->height },
            AssetManager::getAsset<StaticMesh>("UnitCubeMesh"),
            pipeline,
            [cubemap](ProgramPipeline* p) {
                p->setUniform("model", glm::mat4(1.f));
                p->setUniform("view", camera.view());
                p->setUniform("projection", camera.projection());
                p->setUniform("cubemap", 128);
                glBindTextureUnit(128, cubemap);
                // shader->setTexture("cubemap", cubemap);
            },
            GfxPipelineState { }
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
                        glm::dvec2 mouseCursorChange = InputSystem::get()->getMouseCursorChange();
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
#endif
    }

    void Renderer::addDebugDraw(const DebugDraw& debugDraw)
    {
        debugDrawQueue.push(debugDraw);
    }

    void Renderer::drawDebugObjects(SceneRender* render)
    {
        // clear the debug draw render target
        auto debugColor = render->debugColor();

        // hack begin: just a quick and dirty workaround for not having a proper
        // pass for clearing a texture
        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "BlitPS", SHADER_SOURCE_PATH "blit_p.glsl");
        CreatePixelPipeline(p, "BlitQuad", vs, ps);

        drawFullscreenQuad(
            glm::uvec2(debugColor->width, debugColor->height),
            [debugColor](RenderPass& pass) {
                pass.setRenderTarget(debugColor, 0);
            },
            p,
            [](ProgramPipeline* p) {
            }
        );
        // hack end

        while (!debugDrawQueue.empty())
        {
            auto& debugDraw = debugDrawQueue.front();
            debugDraw();
            debugDrawQueue.pop();
        }
    }
}