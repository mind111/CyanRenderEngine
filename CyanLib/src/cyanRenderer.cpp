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
#include "Ray.h"
#include "RenderableScene.h"
#include "Lights.h"
#include "LightComponents.h"
#include "RayTracingScene.h"
#include "IOSystem.h"

#define GPU_RAYTRACING 0

namespace Cyan 
{
    Renderer* Singleton<Renderer>::singleton = nullptr;
    static StaticMesh* fullscreenQuad = nullptr;

    Renderer::IndirectDrawBuffer::IndirectDrawBuffer() 
    {
        glCreateBuffers(1, &buffer);
        glNamedBufferStorage(buffer, sizeInBytes, nullptr, GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT);
        data = glMapNamedBufferRange(buffer, 0, sizeInBytes, GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT);
    }

    Texture2D* Renderer::createRenderTexture(const char* textureName, const Texture2D::Spec& inSpec, const Sampler2D& inSampler)
    {
        auto outTexture = new Texture2D(textureName, inSpec, inSampler);
        outTexture->init();
        renderTextures.push_back(outTexture);
        return outTexture;
    }

    void Renderer::SceneTextures::initialize(const glm::uvec2& inResolution) 
    {
        if (!bInitialized) 
        {
            auto renderer = Renderer::get();
            resolution = inResolution;
            // render target
            renderTarget = renderer->createCachedRenderTarget("Scene", resolution.x, resolution.y);
            // scene color
            Sampler2D sampler;
            {
                Texture2D::Spec spec(resolution.x, resolution.y, 1, PF_RGB16F);
                color = renderer->createRenderTexture("SceneColor", spec, sampler);
            }
            // g-buffer
            {
                {
                    Texture2D::Spec spec(resolution.x, resolution.y, 1, PF_RGB32F);
                    gBuffer.normal = renderer->createRenderTexture("SceneNormalBuffer", spec, sampler);
                    gBuffer.depth = renderer->createRenderTexture("SceneDepthBuffer", spec, sampler);
                }
                {
                    Texture2D::Spec spec(resolution.x, resolution.y, 1, PF_RGB16F);
                    gBuffer.albedo = renderer->createRenderTexture("Albedo", spec, sampler);
                    gBuffer.metallicRoughness = renderer->createRenderTexture("MetallicRoughness", spec, sampler);
                }
            }
            // Hi-Z
            {
                HiZ = new HiZBuffer(gBuffer.depth->getSpec());
                Texture2D::Spec spec(resolution.x, resolution.y, 1, PF_RGB16F);
                ssgiMirror = renderer->createRenderTexture("SSRTMirror", spec, sampler);
            }
            // direct lighting
            {
                Texture2D::Spec spec(resolution.x, resolution.y, 1, PF_RGBA16F);
                directLighting = renderer->createRenderTexture("DirectLighting", spec, sampler);
                directDiffuseLighting = renderer->createRenderTexture("DirectDiffuseLighting", spec, sampler);
            }
            // indirect lighting
            {
                Texture2D::Spec spec(resolution.x, resolution.y, 1, PF_RGB16F);
                ao = renderer->createRenderTexture("SSAO", spec, sampler);
                bentNormal = renderer->createRenderTexture("SSBN", spec, sampler);
                indirectLighting = renderer->createRenderTexture("IndirectLighting", spec, sampler);
                irradiance = renderer->createRenderTexture("Irradiance", spec, sampler);
            }

            Renderer::get()->registerVisualization(std::string("SceneTextures"), color);
            Renderer::get()->registerVisualization(std::string("SceneTextures"), gBuffer.albedo);
            Renderer::get()->registerVisualization(std::string("SceneTextures"), gBuffer.metallicRoughness);
            Renderer::get()->registerVisualization(std::string("SceneTextures"), gBuffer.depth);
            Renderer::get()->registerVisualization(std::string("SceneTextures"), HiZ->texture.get());
            Renderer::get()->registerVisualization(std::string("SceneTextures"), ssgiMirror);
            Renderer::get()->registerVisualization(std::string("SceneTextures"), gBuffer.normal);
            Renderer::get()->registerVisualization(std::string("SceneTextures"), directDiffuseLighting);
            Renderer::get()->registerVisualization(std::string("SceneTextures"), directLighting);
            Renderer::get()->registerVisualization(std::string("SceneTextures"), indirectLighting);
            Renderer::get()->registerVisualization(std::string("SceneTextures"), ao);
            Renderer::get()->registerVisualization(std::string("SceneTextures"), bentNormal);
            Renderer::get()->registerVisualization(std::string("SceneTextures"), irradiance);
            bInitialized = true;
        }
        else if (inResolution != resolution) 
        {
        }
    }

    Renderer::Renderer(GfxContext* ctx, u32 windowWidth, u32 windowHeight)
        : m_ctx(ctx),
        m_windowSize(windowWidth, windowHeight),
        m_frameAllocator(1024 * 1024 * 32),
        m_ssgi(this, glm::uvec2(windowWidth, windowHeight))
    {
        m_manyViewGI = std::make_unique<ManyViewGI>(this, m_ctx);
    }

    void Renderer::initialize() {
        m_manyViewGI->initialize();
    };

    void Renderer::deinitialize() {
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

    void Renderer::appendToRenderingTab(const std::function<void()>& command) {
        ImGui::Begin("Cyan", nullptr);
        {
            ImGui::BeginTabBar("##Views");
            {
                if (ImGui::BeginTabItem("Rendering"))
                {
                    ImGui::BeginChild("##Rendering Settings", ImVec2(0, 0), true); 
                    {
                        command();
                        ImGui::EndChild();
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

// todo: fix this code branch at some point
#if 0
    void Renderer::drawMeshInstance(const RenderableScene& scene, RenderTarget* renderTarget, Viewport viewport, GfxPipelineState pipelineState, MeshInstance* meshInstance, i32 transformIndex) {
        Mesh* parent = meshInstance->parent;
        for (u32 i = 0; i < parent->numSubmeshes(); ++i) {
            Material* material = meshInstance->getMaterial(i);
            if (material) {
                RenderTask task = { };
                task.renderTarget = renderTarget;
                task.viewport = viewport;
                task.shader = material->get();
                task.submesh = parent->getSubmesh(i);
                task.renderSetupLambda = [this, &scene, transformIndex, material](RenderTarget* renderTarget, Shader* shader) {
                    material->setShaderMaterialParameters();
                    shader->setUniform("transformIndex", transformIndex);
                };

                submitRenderTask(std::move(task));
            }
        }
    }
#endif

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

    void Renderer::drawScreenQuad(RenderTarget* renderTarget, Viewport viewport, PixelPipeline* pipeline, RenderSetupLambda&& renderSetupLambda) {
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
        auto va = task.submesh.va;
        m_ctx->setVertexArray(va);
        if (va->hasIndexBuffer()) 
        {
            m_ctx->drawIndex(task.submesh.numIndices());
        }
        else 
        {
            m_ctx->drawIndexAuto(task.submesh.numVertices());
        }
    }

    void Renderer::registerVisualization(const std::string& categoryName, Texture2D* visualization, bool* toggle) 
    {
        auto entry = visualizationMap.find(categoryName);
        if (entry == visualizationMap.end()) 
        {
            visualizationMap.insert({ categoryName, { VisualizationDesc{ visualization, 0, toggle } } });
        }
        else 
        {
            entry->second.push_back(VisualizationDesc{ visualization, 0, toggle });
        }
    }

    void Renderer::beginRender() 
    {
        // reset frame allocator
        m_frameAllocator.reset();
    }

    void Renderer::render(Scene* scene, const SceneView& sceneView, const glm::uvec2& renderResolution) 
    {
        beginRender();
        {
            // shared render target for this frame
            m_sceneTextures.initialize(renderResolution);

            // convert Scene instance to RenderableScene instance for rendering
#if BINDLESS_TEXTURE
            RenderableSceneBindless renderableScene(scene, sceneView, m_frameAllocator);
#else
            RenderableSceneTextureAtlas renderableScene(scene, sceneView, m_frameAllocator);
#endif

            // shadow
            renderShadowMaps(renderableScene);

            // depth prepass
            renderSceneDepthPrepass(renderableScene, m_sceneTextures.renderTarget, m_sceneTextures.gBuffer.depth);

            // main scene pass
#if BINDLESS_TEXTURE
            renderSceneGBuffer(m_sceneTextures.renderTarget, renderableScene, m_sceneTextures.gBuffer);
#else
            renderSceneGBufferWithTextureAtlas(m_sceneTextures.renderTarget, renderableScene, m_sceneTextures.gBuffer);
#endif
            renderSceneLighting(m_sceneTextures.renderTarget, m_sceneTextures.color, renderableScene, m_sceneTextures.gBuffer);
            // m_manyViewGI->render(m_sceneTextures.renderTarget, renderableScene, m_sceneTextures.gBuffer.depth, m_sceneTextures.gBuffer.normal);

            // draw debug objects if any
            drawDebugObjects();

            // post processing
            if (m_settings.bPostProcessing) 
            {
                auto bloomTexture = bloom(m_sceneTextures.color);
                compose(sceneView.renderTexture, m_sceneTextures.color, bloomTexture.get(), m_windowSize);
            }

            if (m_visualization)
            {
                visualize(sceneView.renderTexture, m_visualization->texture, m_visualization->activeMip);
            }
        } 
        endRender();
    }

    void Renderer::visualize(Texture2D* dst, Texture2D* src, i32 mip) 
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

    void Renderer::renderToScreen(Texture2D* inTexture)
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
        m_numFrames++;
    }

    void Renderer::renderShadowMaps(RenderableScene& inScene) {
        // make a copy
#if BINDLESS_TEXTURE
        RenderableSceneBindless scene(inScene);
#else
#endif
        for (i32 i = 0; i < inScene.directionalLights.size(); ++i) {
            if (inScene.directionalLights[i]->bCastShadow) {
                inScene.directionalLights[i]->renderShadowMap(scene, this);
                if (auto directionalLight = dynamic_cast<CSMDirectionalLight*>(inScene.directionalLights[i])) {
                    (*inScene.directionalLightBuffer)[i] = directionalLight->buildGpuLight();
                }
            }
        }
        // todo: point light
    }

    void Renderer::renderSceneDepthPrepass(RenderableScene& scene, RenderTarget* outRenderTarget, Texture2D* outDepthTexture)
    {
        outRenderTarget->setColorBuffer(outDepthTexture, 0);
        outRenderTarget->setDrawBuffers({ 0 });
        outRenderTarget->clearDrawBuffer(0, glm::vec4(1.f, 1.f, 1.f, 1.f));
        m_ctx->setRenderTarget(outRenderTarget);
        m_ctx->setViewport({ 0, 0, outRenderTarget->width, outRenderTarget->height });
        CreateVS(vs, "SceneDepthPrepassVS", SHADER_SOURCE_PATH "scene_depth_prepass_v.glsl");
        CreatePS(ps, "SceneDepthPrepassPS", SHADER_SOURCE_PATH "scene_depth_prepass_p.glsl");
        CreatePixelPipeline(pipeline, "SceneDepthPrepass", vs, ps);
        m_ctx->setPixelPipeline(pipeline);
        m_ctx->setDepthControl(DepthControl::kEnable);
        scene.upload();
        multiDrawSceneIndirect(scene);
    }

    void Renderer::renderSceneDepthOnly(RenderableScene& scene, DepthTexture2D* outDepthTexture) 
    {
        std::unique_ptr<RenderTarget> depthRenderTarget(createDepthOnlyRenderTarget(outDepthTexture->width, outDepthTexture->height));
        depthRenderTarget->setDepthBuffer(reinterpret_cast<DepthTexture2D*>(outDepthTexture));
        depthRenderTarget->clear({ { 0u } });
        m_ctx->setRenderTarget(depthRenderTarget.get());
        m_ctx->setViewport({ 0, 0, depthRenderTarget->width, depthRenderTarget->height });
        CreateVS(vs, "DepthOnlyVS", SHADER_SOURCE_PATH "depth_only_v.glsl");
        CreatePS(ps, "DepthOnlyPS", SHADER_SOURCE_PATH "depth_only_p.glsl");
        CreatePixelPipeline(pipeline, "DepthOnly", vs, ps);
        scene.upload();
        m_ctx->setPixelPipeline(pipeline);
        m_ctx->setDepthControl(DepthControl::kEnable);
        multiDrawSceneIndirect(scene);
    }

    void Renderer::renderSceneDepthNormal(RenderableScene& scene, RenderTarget* outRenderTarget, Texture2D* outDepthBuffer, Texture2D* outNormalBuffer) 
    {
        outRenderTarget->setColorBuffer(outDepthBuffer, 0);
        outRenderTarget->setColorBuffer(outNormalBuffer, 1);
        outRenderTarget->setDrawBuffers({ 0, 1 });
        outRenderTarget->clearDrawBuffer(0, glm::vec4(1.f));
        outRenderTarget->clearDrawBuffer(1, glm::vec4(0.f, 0.f, 0.f, 1.f));

        m_ctx->setRenderTarget(outRenderTarget);
        m_ctx->setViewport({ 0, 0, outRenderTarget->width, outRenderTarget->height });
        CreateVS(vs, "SceneDepthNormalVS", SHADER_SOURCE_PATH "scene_depth_normal_v.glsl");
        CreatePS(ps, "SceneDepthNormalPS", SHADER_SOURCE_PATH "scene_depth_normal_p.glsl");
        CreatePixelPipeline(pipeline, "SceneDepthNormal", vs, ps);
        m_ctx->setPixelPipeline(pipeline);
        m_ctx->setDepthControl(DepthControl::kEnable);
        scene.upload();
        multiDrawSceneIndirect(scene);
    }

    void Renderer::renderSceneGBuffer(RenderTarget* outRenderTarget, RenderableScene& scene, GBuffer gBuffer)
    {
        CreateVS(vs, "SceneColorPassVS", SHADER_SOURCE_PATH "scene_pass_v.glsl");
        CreatePS(ps, "SceneGBufferPassPS", SHADER_SOURCE_PATH "scene_gbuffer_p.glsl");
        CreatePixelPipeline(pipeline, "SceneGBufferPass", vs, ps);
        m_ctx->setPixelPipeline(pipeline, [](VertexShader* vs, PixelShader* ps) {

        });

        outRenderTarget->setColorBuffer(gBuffer.albedo, 0);
        outRenderTarget->setColorBuffer(gBuffer.normal, 1);
        outRenderTarget->setColorBuffer(gBuffer.metallicRoughness, 2);
        outRenderTarget->setDrawBuffers({ 0, 1, 2 });
        outRenderTarget->clearDrawBuffer(0, glm::vec4(0.f, 0.f, 0.f, 1.f), false);
        outRenderTarget->clearDrawBuffer(1, glm::vec4(0.f, 0.f, 0.f, 1.f), false);
        outRenderTarget->clearDrawBuffer(2, glm::vec4(0.f, 0.f, 0.f, 1.f), false);

        m_ctx->setRenderTarget(outRenderTarget);
        m_ctx->setViewport({ 0, 0, outRenderTarget->width, outRenderTarget->height });
        m_ctx->setDepthControl(DepthControl::kEnable);
        scene.upload();
        multiDrawSceneIndirect(scene);
    }

    void Renderer::renderSceneLighting(RenderTarget* outRenderTarget, Texture2D* outSceneColor, RenderableScene& scene, GBuffer gBuffer)
    {
        renderSceneDirectLighting(outRenderTarget, m_sceneTextures.directLighting, scene, m_sceneTextures.gBuffer);
        renderSceneIndirectLighting(outRenderTarget, m_sceneTextures.indirectLighting, scene, m_sceneTextures.gBuffer);

        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "SceneLightingPassPS", SHADER_SOURCE_PATH "scene_lighting_p.glsl");
        CreatePixelPipeline(pipeline, "SceneLightingPass", vs, ps);

        outRenderTarget->setColorBuffer(m_sceneTextures.color, 0);
        outRenderTarget->setDrawBuffers({ 0 });
        outRenderTarget->clearDrawBuffer(0, glm::vec4(0.f, 0.f, 0.f, 1.f), false);

        // combine direct lighting with indirect lighting
        drawFullscreenQuad(
            outRenderTarget,
            pipeline,
            [this](VertexShader* vs, PixelShader* ps) {
                ps->setTexture("directLightingTexture", m_sceneTextures.directLighting);
                ps->setTexture("indirectLightingTexture", m_sceneTextures.indirectLighting);
            }
        );
    }

    void Renderer::renderSceneDirectLighting(RenderTarget* outRenderTarget, Texture2D* outSceneDirectLighting, RenderableScene& scene, GBuffer gBuffer)
    {
        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "SceneDirectLightingPS", SHADER_SOURCE_PATH "scene_direct_lighting_p.glsl");
        CreatePixelPipeline(pipeline, "SceneDirectLightingPass", vs, ps);

        outRenderTarget->setColorBuffer(outSceneDirectLighting, 0);
        outRenderTarget->setColorBuffer(m_sceneTextures.directDiffuseLighting, 1);
        outRenderTarget->setDrawBuffers({ 0, 1 });
        outRenderTarget->clearDrawBuffer({ 0 }, glm::vec4(0.f, 0.f, 0.f, 1.f), false);
        outRenderTarget->clearDrawBuffer({ 1 }, glm::vec4(0.f, 0.f, 0.f, 1.f), false);

        drawFullscreenQuad(outRenderTarget, pipeline, [gBuffer](VertexShader* vs, PixelShader* ps) {
            ps->setTexture("sceneDepth", gBuffer.depth);
            ps->setTexture("sceneNormal", gBuffer.normal);
            ps->setTexture("sceneAlbedo", gBuffer.albedo);
            ps->setTexture("sceneMetallicRoughness", gBuffer.metallicRoughness);
        });

        if (scene.skybox)
        {
            scene.skybox->render(outRenderTarget, scene.camera.view, scene.camera.projection);
        }
    }

    void Renderer::renderSceneIndirectLighting(RenderTarget* outRenderTarget, Texture2D* outIndirectLighting, RenderableScene& scene, GBuffer gBuffer)
    {
        // render AO and bent normal, as well as indirect irradiance
        if (bDebugSSRT)
        {
            visualizeSSRT(gBuffer.depth, gBuffer.normal);
        }
        m_ssgi.renderEx(m_sceneTextures.ao, m_sceneTextures.bentNormal, m_sceneTextures.irradiance, m_sceneTextures.gBuffer, m_sceneTextures.HiZ, m_sceneTextures.directDiffuseLighting);

        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "SceneIndirectLightingPass", SHADER_SOURCE_PATH "scene_indirect_lighting_p.glsl");
        CreatePixelPipeline(pipeline, "SceneIndirectLightingPass", vs, ps);

        outRenderTarget->setColorBuffer(outIndirectLighting, 0);
        outRenderTarget->setDrawBuffers({ 0 });
        outRenderTarget->clearDrawBuffer(0, glm::vec4(0.f, 0.f, 0.f, 1.f), false);

        drawFullscreenQuad(outRenderTarget, pipeline, [this, &scene, gBuffer](VertexShader* vs, PixelShader* ps) {
            ps->setTexture("sceneDepth", gBuffer.depth);
            ps->setTexture("sceneNormal", gBuffer.normal);
            ps->setTexture("sceneAlbedo", gBuffer.albedo);
            ps->setTexture("sceneMetallicRoughness", gBuffer.metallicRoughness);
            // setup ssao
            if (m_sceneTextures.ao)
            {
                auto ssao = m_sceneTextures.ao->glHandle;
                if (glIsTextureHandleResidentARB(ssao) == GL_FALSE)
                {
                    glMakeTextureHandleResidentARB(ssao);
                }
                ps->setUniform("ssaoTextureHandle", ssao);
            }
            ps->setUniform("ssaoEnabled", m_settings.bSSAOEnabled && m_sceneTextures.ao ? 1.f : 0.f);
            // setup ssbn
            if (m_sceneTextures.bentNormal)
            {
                auto ssbn = m_sceneTextures.bentNormal->glHandle;
                if (glIsTextureHandleResidentARB(ssbn) == GL_FALSE)
                {
                    glMakeTextureHandleResidentARB(ssbn);
                }
                ps->setUniform("ssbnTextureHandle", ssbn);
            }
            ps->setUniform("ssbnEnabled", m_settings.bBentNormalEnabled && m_sceneTextures.bentNormal ? 1.f : 0.f);
            // setup indirect irradiance
            ps->setTexture("indirectIrradiance", m_sceneTextures.irradiance);
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
                if (glIsTextureHandleResidentARB(BRDFLookupTexture) == GL_FALSE) {
                    glMakeTextureHandleResidentARB(BRDFLookupTexture);
                }
                ps->setUniform("skyLight.BRDFLookupTexture", BRDFLookupTexture);
            }
        });
    }

    HiZBuffer::HiZBuffer(const Texture2D::Spec& inSpec)
    {
        // for now, we should be using square render target with power of 2 resolution
        assert(isPowerOf2(inSpec.width) && isPowerOf2(inSpec.height));
        assert(inSpec.width == inSpec.height);
        Texture2D::Spec spec(inSpec);
        spec.numMips = log2(spec.width) + 1;
        Sampler2D sampler;
        sampler.magFilter = FM_POINT;
        sampler.minFilter = FM_MIPMAP_POINT;
        auto renderer = Renderer::get();
        texture = std::unique_ptr<Texture2D>(renderer->createRenderTexture("HiZBuffer", spec, sampler));
    }

    void HiZBuffer::build(Texture2D* srcDepthTexture)
    {
        auto renderer = Renderer::get();
        // copy pass
        {
            auto renderTarget = renderer->createCachedRenderTarget("HiZ_Mip[0]", texture->width, texture->height);
            renderTarget->setColorBuffer(texture.get(), 0, 0);
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
            u32 mipWidth = texture->width;
            u32 mipHeight = texture->height;
            for (i32 i = 1; i < texture->numMips; ++i)
            {
                mipWidth /= 2u;
                mipHeight /= 2u;
                const i32 kMaxNameLen = 128;
                char name[kMaxNameLen];
                sprintf_s(name, "HiZ_Pass[%d]", i);
                auto src = i - 1;
                auto dst = i;
                auto renderTarget = renderer->createCachedRenderTarget(name, mipWidth, mipHeight);
                renderTarget->setColorBuffer(texture.get(), 0, (u32)dst);
                renderer->drawFullscreenQuad(
                    renderTarget,
                    pipeline,
                    [this, src](VertexShader* vs, PixelShader* ps) {
                        ps->setUniform("srcMip", src);
                        ps->setTexture("srcDepthTexture", texture.get());
                    }
                );
            }
        }
    }

    void Renderer::visualizeSSRT(Texture2D* depth, Texture2D* normal)
    {
        // build Hi-Z buffer
        {
            m_sceneTextures.HiZ->build(depth);
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
        static ShaderStorageBuffer<DynamicSsboData<DebugTraceData>> debugTraceBuffer("DebugTraceBuffer", kNumIterations);
        static ShaderStorageBuffer<DynamicSsboData<Vertex>> debugRayBuffer("DebugRayBuffer", numDebugRays * kNumIterations);

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
                    cs->setTexture("HiZ", m_sceneTextures.HiZ->texture.get());
                    auto blueNoiseTexture = AssetManager::getAsset<Texture2D>("BlueNoise_1024x1024");
                    cs->setTexture("blueNoiseTexture", blueNoiseTexture);
                    cs->setUniform("outputSize", glm::vec2(depth->width, depth->height));
                    cs->setUniform("numLevels", (i32)m_sceneTextures.HiZ->texture->numMips);
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
                memset(debugTraceBuffer.data.array.data(), 0x0, debugTraceBuffer.data.getDynamicDataSizeInBytes());
                glGetNamedBufferSubData(debugTraceBuffer.getGpuObject(), 0, debugTraceBuffer.data.getDynamicDataSizeInBytes(), debugTraceBuffer.data.array.data());
                memset(debugRayBuffer.data.array.data(), 0x0, debugRayBuffer.data.getDynamicDataSizeInBytes());
                glGetNamedBufferSubData(debugRayBuffer.getGpuObject(), 0, debugRayBuffer.data.getSizeInBytes(), debugRayBuffer.data.array.data());
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
                    if (debugRayBuffer[index].position.w > 0.f)
                    {
                        vertices.push_back(debugRayBuffer[index]);
                    }
                }
                // visualize the debug ray
                {
                    drawWorldSpaceLines(m_sceneTextures.renderTarget, vertices);
                    drawWorldSpacePoints(m_sceneTextures.renderTarget, vertices);
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
                auto renderTarget = createCachedRenderTarget("SSRTDebugMirror", m_sceneTextures.renderTarget->width, m_sceneTextures.renderTarget->height);
                renderTarget->setColorBuffer(m_sceneTextures.ssgiMirror, 0);
                renderTarget->setDrawBuffers({ 0 });
                renderTarget->clearDrawBuffer({ 0 }, glm::vec4(0.f, 0.f, 0.f, 1.f), false);
                drawFullscreenQuad(
                    renderTarget,
                    pipeline,
                    [this](VertexShader* vs, PixelShader* ps) {
                        ps->setTexture("sceneDepth", m_sceneTextures.gBuffer.depth);
                        ps->setTexture("sceneNormal", m_sceneTextures.gBuffer.normal);
                        ps->setTexture("directDiffuseBuffer", m_sceneTextures.gBuffer.normal);
                        ps->setUniform("numLevels", (i32)m_sceneTextures.HiZ->texture->numMips);
                        ps->setTexture("HiZ", m_sceneTextures.HiZ->texture.get());
                        ps->setUniform("kMaxNumIterations", kNumIterations);
                    }
                );
            }
        }
    }

    // todo: try to defer drawing debug objects till after the post-processing pass
    void Renderer::drawWorldSpacePoints(RenderTarget* renderTarget, const std::vector<Vertex>& points)
    {
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
    }

    void Renderer::drawWorldSpaceLines(RenderTarget* renderTarget, const std::vector<Vertex>& vertices)
    {
        static ShaderStorageBuffer<DynamicSsboData<Vertex>> vertexBuffer("VertexBuffer", 1024);

        debugDrawCalls.push([this, renderTarget, vertices]() {
                // setup buffer
                u32 numVertices = vertices.size();
                u32 numLineSegments = max(i32(numVertices - 1), (i32)0);
                u32 numVerticesToDraw = numLineSegments * 2;
                assert(vertices.size() < vertexBuffer.getNumElements());
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
    }

    void Renderer::drawScreenSpaceLines(RenderTarget* renderTarget, const std::vector<Vertex>& vertices)
    {
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

    void Renderer::legacyScreenSpaceRayTracing(Texture2D* depth, Texture2D* normal)
    {
        auto renderTarget = createCachedRenderTarget("ScreenSpaceRayTracing", depth->width, depth->height);
        renderTarget->setColorBuffer(m_sceneTextures.ao, 0);
        renderTarget->setColorBuffer(m_sceneTextures.bentNormal, 1);
        renderTarget->setDrawBuffers({ 0, 1, });
        renderTarget->clear({ 
            { 0, glm::vec4(0.f, 0.f, 0.f, 1.f) },
            { 1, glm::vec4(0.f, 0.f, 0.f, 1.f) },
        });
        CreateVS(vs, "ScreenSpaceRayTracingVS", SHADER_SOURCE_PATH "screenspace_raytracing_v.glsl");
        CreatePS(ps, "LegacySSRTPS", SHADER_SOURCE_PATH "screenspace_raytracing_p.glsl");
        CreatePixelPipeline(pipeline, "SSRT", vs, ps);
        drawFullscreenQuad(
            renderTarget,
            pipeline,
            [this, depth, normal](VertexShader* vs, PixelShader* ps) {
                ps->setUniform("outputSize", glm::vec2(depth->width, depth->height));
                ps->setTexture("depthTexture", depth);
                ps->setTexture("normalTexture", normal);
                auto blueNoiseTexture = AssetManager::getAsset<Texture2D>("BlueNoise_1024x1024");
                ps->setTexture("blueNoiseTexture", blueNoiseTexture);
            }
        );
    }

    Renderer::SSGI::HitBuffer::HitBuffer(u32 inNumLayers, const glm::uvec2& resolution)
        : numLayers(inNumLayers)
    {
        Texture2DArray::Spec spec(resolution.x, resolution.y, 1, numLayers, PF_RGBA16F);
        position = new Texture2DArray("SSRTHitPositionBuffer", spec, Sampler2D());
        position->init();
        normal = new Texture2DArray("SSRTHitNormalBuffer", spec, Sampler2D());
        normal->init();
        radiance = new Texture2DArray("SSRTHitRadianceBuffer", spec, Sampler2D());
        radiance->init();
    }

    Renderer::SSGI::SSGI(Renderer* inRenderer, const glm::uvec2& inRes)
        : resolution(inRes), hitBuffer(kNumSamples, inRes), renderer(inRenderer)
    {
    }

    void Renderer::SSGI::render(Texture2D* outAO, Texture2D* outBentNormal, Texture2D* outIrradiance, const Renderer::GBuffer& gBuffer, HiZBuffer* HiZ, Texture2D* inDirectDiffuseBuffer)
    {
        // build Hi-Z buffer
        {
            HiZ->build(gBuffer.depth);
        }
        // trace
        auto renderTarget = renderer->createCachedRenderTarget("ScreenSpaceRayTracing", gBuffer.depth->width, gBuffer.depth->height);
        renderTarget->setColorBuffer(outAO, 0);
        renderTarget->setColorBuffer(outBentNormal, 1);
        renderTarget->setColorBuffer(outIrradiance, 2);
        renderTarget->setDrawBuffers({ 0, 1, 2 });
        renderTarget->clear({ 
            { 0, glm::vec4(0.f, 0.f, 0.f, 1.f) },
            { 1, glm::vec4(0.f, 0.f, 0.f, 1.f) },
            { 2, glm::vec4(0.f, 0.f, 0.f, 1.f) },
        });
        CreateVS(vs, "ScreenSpaceRayTracingVS", SHADER_SOURCE_PATH "screenspace_raytracing_v.glsl");
        CreatePS(ps, "HierarchicalSSRTPS", SHADER_SOURCE_PATH "hierarchical_ssrt_p.glsl");
        CreatePixelPipeline(pipeline, "HierarchicalSSRT", vs, ps);
        renderer->drawFullscreenQuad(
            renderTarget,
            pipeline,
            [this, gBuffer, HiZ, inDirectDiffuseBuffer](VertexShader* vs, PixelShader* ps) {
                ps->setUniform("outputSize", glm::vec2(gBuffer.depth->width, gBuffer.depth->height));
                ps->setTexture("depthBuffer", gBuffer.depth);
                ps->setTexture("normalBuffer", gBuffer.normal);
                ps->setTexture("HiZ", HiZ->texture.get());
                ps->setUniform("numLevels", (i32)HiZ->texture->numMips);
                ps->setUniform("kMaxNumIterations", (i32)kNumIterations);
                auto blueNoiseTexture = AssetManager::getAsset<Texture2D>("BlueNoise_1024x1024");
                ps->setTexture("blueNoiseTexture", blueNoiseTexture);
                ps->setTexture("directLightingBuffer", inDirectDiffuseBuffer);
            }
        );
    }

    void Renderer::SSGI::renderEx(Texture2D* outAO, Texture2D* outBentNormal, Texture2D* outIrradiance, const Renderer::GBuffer& gBuffer, HiZBuffer* HiZ, Texture2D* inDirectDiffuseBuffer)
    {
        // build Hi-Z buffer
        {
            HiZ->build(gBuffer.depth);
        }
        // trace
        auto renderTarget = renderer->createCachedRenderTarget("ScreenSpaceRayTracing", gBuffer.depth->width, gBuffer.depth->height);
        renderTarget->setColorBuffer(outAO, 0);
        renderTarget->setColorBuffer(outBentNormal, 1);
        renderTarget->setColorBuffer(outIrradiance, 2);
        renderTarget->setDrawBuffers({ 0, 1, 2 });
        renderTarget->clear({ 
            { 0, glm::vec4(0.f, 0.f, 0.f, 1.f) },
            { 1, glm::vec4(0.f, 0.f, 0.f, 1.f) },
            { 2, glm::vec4(0.f, 0.f, 0.f, 1.f) },
        });
        CreateVS(vs, "ScreenSpaceRayTracingVS", SHADER_SOURCE_PATH "screenspace_raytracing_v.glsl");
        CreatePS(ps, "HierarchicalSSRTPS", SHADER_SOURCE_PATH "hierarchical_ssrt_ex_p.glsl");
        CreatePixelPipeline(pipeline, "HierarchicalSSRT", vs, ps);
        renderer->drawFullscreenQuad(
            renderTarget,
            pipeline,
            [this, gBuffer, HiZ, inDirectDiffuseBuffer](VertexShader* vs, PixelShader* ps) {
                ps->setUniform("outputSize", glm::vec2(gBuffer.depth->width, gBuffer.depth->height));
                ps->setTexture("depthBuffer", gBuffer.depth);
                ps->setTexture("normalBuffer", gBuffer.normal);
                ps->setTexture("HiZ", HiZ->texture.get());
                ps->setUniform("numLevels", (i32)HiZ->texture->numMips);
                ps->setUniform("kMaxNumIterations", (i32)kNumIterations);
                auto blueNoiseTexture = AssetManager::getAsset<Texture2D>("BlueNoise_1024x1024");
                ps->setTexture("blueNoiseTexture", blueNoiseTexture);
                ps->setTexture("directLightingBuffer", inDirectDiffuseBuffer);
                ps->setUniform("numSamples", (i32)kNumSamples);

                glBindImageTexture(0, hitBuffer.position->getGpuObject(), 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
                glBindImageTexture(1, hitBuffer.normal->getGpuObject(), 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
                glBindImageTexture(2, hitBuffer.radiance->getGpuObject(), 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
            }
        );
        // final resolve pass
        {
            CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            CreatePS(ps, "SSRTResolvePS", SHADER_SOURCE_PATH "ssrt_resolve_p.glsl");
            CreatePixelPipeline(pipeline, "SSRTResolve", vs, ps);
            renderer->drawFullscreenQuad(
                renderTarget,
                pipeline,
                [this, gBuffer](VertexShader* vs, PixelShader* ps) {
                    ps->setTexture("depthBuffer", gBuffer.depth);
                    ps->setTexture("normalBuffer", gBuffer.normal);
                    ps->setTexture("hitPositionBuffer", hitBuffer.position);
                    ps->setTexture("hitNormalBuffer", hitBuffer.normal);
                    ps->setTexture("hitRadianceBuffer", hitBuffer.radiance);
                    auto blueNoiseTexture = AssetManager::getAsset<Texture2D>("BlueNoise_1024x1024");
                    ps->setTexture("blueNoiseTexture", blueNoiseTexture);
                    ps->setUniform("reuseKernelRadius", reuseKernelRadius);
                    ps->setUniform("numReuseSamples", numReuseSamples);
                }
            );
        }
    }

#if 0
    Texture2D* Renderer::renderScene(RenderableScene& scene, const SceneView& sceneView, const glm::uvec2& outputResolution) {
        ITexture::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = outputResolution.x;
        spec.height = outputResolution.y;
        spec.pixelFormat = PF_RGB16F;
        static Texture2D* outSceneColor = new Texture2D("SceneColor", spec);

        std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(outSceneColor->width, outSceneColor->height));
        renderTarget->setColorBuffer(outSceneColor, 0);
        renderTarget->clear({ { 0 } });

        scene.upload();

        // render mesh instances
        u32 transformIndex = 0u;
        for (auto meshInst : scene.meshInstances)
        {
            drawMeshInstance(
                scene,
                renderTarget.get(),
                { 0u, 0u, outSceneColor->width, outSceneColor->height },
                GfxPipelineState(), 
                meshInst,
                transformIndex++
            );
        }

        // render skybox
        if (scene.skybox) { 
            scene.skybox->render(renderTarget.get());
        }

        return outSceneColor;
    }
#endif

    void Renderer::multiDrawSceneIndirect(const RenderableScene& scene) 
    {
        struct IndirectDrawArrayCommand
        {
            u32  count;
            u32  instanceCount;
            u32  first;
            i32  baseInstance;
        };
        // build indirect draw commands
        auto ptr = reinterpret_cast<IndirectDrawArrayCommand*>(indirectDrawBuffer.data);
        auto& submeshBuffer = StaticMesh::getSubmeshBuffer();
        for (i32 draw = 0; draw < scene.drawCallBuffer->getNumElements() - 1; ++draw)
        {
            IndirectDrawArrayCommand& command = ptr[draw];
            command.first = 0;
            u32 instance = (*scene.drawCallBuffer)[draw];
            u32 submesh = (*scene.instanceBuffer)[instance].submesh;
            // command.count = RenderableScene::packedGeometry->submeshes[submesh].numIndices;
            command.count = submeshBuffer[submesh].numIndices;
            command.instanceCount = (*scene.drawCallBuffer)[draw + 1] - (*scene.drawCallBuffer)[draw];
            command.baseInstance = 0;
        }

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectDrawBuffer.buffer);
        // dispatch multi draw indirect
        // one sub-drawcall per instance
        u32 drawCount = max(scene.drawCallBuffer->getNumElements() - 1, 0);
        glMultiDrawArraysIndirect(GL_TRIANGLES, 0, drawCount, 0);
    }

    void Renderer::renderSceneBatched(RenderableScene& scene, RenderTarget* outRenderTarget, Texture2D* outSceneColor) 
    {
        CreateVS(vs, "SceneColorPassVS", SHADER_SOURCE_PATH "scene_pass_v.glsl");
        CreatePS(ps, "SceneColorPassPS", SHADER_SOURCE_PATH "scene_pass_p.glsl");
        CreatePixelPipeline(pipeline, "SceneColorPass", vs, ps);
        m_ctx->setPixelPipeline(pipeline, [this, &scene, outSceneColor](VertexShader* vs, PixelShader* ps) {
            ps->setUniform("outputSize", glm::vec2(outSceneColor->width, outSceneColor->height));
            // setup ssao
            if (m_settings.bSSAOEnabled)
            {
                if (m_sceneTextures.ao)
                {
                    auto ssao = m_sceneTextures.ao->glHandle;
                    if (glIsTextureHandleResidentARB(ssao) == GL_FALSE)
                    {
                        glMakeTextureHandleResidentARB(ssao);
                    }
                    ps->setUniform("ssaoTextureHandle", ssao);
                    ps->setUniform("ssaoEnabled", 1.f);
                }
            }
            else
            {
                ps->setUniform("ssaoEnabled", 0.f);
            }
            if (m_settings.bBentNormalEnabled)
            {
                // setup ssbn
                if (m_sceneTextures.bentNormal)
                {
                    auto ssbn = m_sceneTextures.bentNormal->glHandle;
                    if (glIsTextureHandleResidentARB(ssbn) == GL_FALSE)
                    {
                        glMakeTextureHandleResidentARB(ssbn);
                    }
                    ps->setUniform("ssbnTextureHandle", ssbn);
                    ps->setUniform("ssbnEnabled", 1.f);
                }
            }
            else
            {
                ps->setUniform("ssbnEnabled", 0.f);
            }

            // sky light
            /* note
            * seamless cubemap doesn't work with bindless textures that's accessed using a texture handle,
            * so falling back to normal way of binding textures here.
            */
            ps->setTexture("skyLight.irradiance", scene.skyLight->irradianceProbe->m_convolvedIrradianceTexture.get());
            ps->setTexture("skyLight.reflection", scene.skyLight->reflectionProbe->m_convolvedReflectionTexture.get());
            auto BRDFLookupTexture = ReflectionProbe::getBRDFLookupTexture()->glHandle;
            if (glIsTextureHandleResidentARB(BRDFLookupTexture) == GL_FALSE) {
                glMakeTextureHandleResidentARB(BRDFLookupTexture);
            }
            ps->setUniform("skyLight.BRDFLookupTexture", BRDFLookupTexture);
        });
        outRenderTarget->setColorBuffer(outSceneColor, 0);
        outRenderTarget->setDrawBuffers({ 0 });
        outRenderTarget->clearDrawBuffer(0, glm::vec4(0.f, 0.f, 0.f, 1.f), false);
        m_ctx->setRenderTarget(outRenderTarget);
        m_ctx->setViewport({ 0, 0, outRenderTarget->width, outRenderTarget->height });
        m_ctx->setDepthControl(DepthControl::kEnable);
        scene.upload();
        multiDrawSceneIndirect(scene);

        // render skybox
        if (scene.skybox) 
        {
            scene.skybox->render(outRenderTarget, scene.camera.view, scene.camera.projection);
        }
    }

    void Renderer::renderSceneGBufferWithTextureAtlas(RenderTarget* outRenderTarget, RenderableScene& scene, GBuffer gBuffer)
    {
        CreateVS(vs, "SceneColorPassVS", SHADER_SOURCE_PATH "scene_pass_v.glsl");
        CreatePS(ps, "SceneGBufferWithTextureAtlasPassPS", SHADER_SOURCE_PATH "scene_gbuffer_texture_atlas_p.glsl");
        CreatePixelPipeline(pipeline, "SceneGBufferWithTextureAtlasPass", vs, ps);
        m_ctx->setPixelPipeline(pipeline, [](VertexShader* vs, PixelShader* ps) {
        });

        outRenderTarget->setColorBuffer(gBuffer.albedo, 0);
        outRenderTarget->setColorBuffer(gBuffer.normal, 1);
        outRenderTarget->setColorBuffer(gBuffer.metallicRoughness, 2);
        outRenderTarget->setDrawBuffers({ 0, 1, 2 });
        outRenderTarget->clearDrawBuffer(0, glm::vec4(0.f, 0.f, 0.f, 1.f), false);
        outRenderTarget->clearDrawBuffer(1, glm::vec4(0.f, 0.f, 0.f, 1.f), false);
        outRenderTarget->clearDrawBuffer(2, glm::vec4(0.f, 0.f, 0.f, 1.f), false);

        m_ctx->setRenderTarget(outRenderTarget);
        m_ctx->setViewport({ 0, 0, outRenderTarget->width, outRenderTarget->height });
        m_ctx->setDepthControl(DepthControl::kEnable);
        scene.upload();
        multiDrawSceneIndirect(scene);
    }

    void Renderer::downsample(Texture2D* src, Texture2D* dst) {
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

    void Renderer::upscale(Texture2D* src, Texture2D* dst)
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

    std::unordered_multimap<Texture2D::Spec, Texture2D*> CachedTexture2D::cache;

    CachedTexture2D Renderer::bloom(Texture2D* src)
    {
        // setup pass
        CachedTexture2D bloomSetupTexture("BloomSetup", src->getSpec());
        auto renderTarget = createCachedRenderTarget("BloomSetup", bloomSetupTexture->width, bloomSetupTexture->height);
        renderTarget->setColorBuffer(bloomSetupTexture.get(), 0);
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
        CachedTexture2D downsamplePyramid[numPasses + 1] = { };
        // downsample passes
        {
            downsamplePyramid[0] = bloomSetupTexture;
            for (i32 pass = 1; pass <= numPasses; ++pass)
            { 
                Texture2D* src = downsamplePyramid[pass - 1].get();
                std::string passName("BloomDownsample");
                passName += '[' + std::to_string(pass) + ']';
                Texture2D::Spec spec = src->getSpec();
                spec.width /= 2;
                spec.height /= 2;
                if (spec.width == 0u || spec.height == 0u)
                {
                    assert(0);
                }
                // todo: need to decouple Sampler from texture!!!
                // todo: this is a bug here, I shouldn't use point sampling here
                downsamplePyramid[pass] = CachedTexture2D(passName.c_str(), spec);
                Texture2D* dst = downsamplePyramid[pass].get();
                downsample(src, dst);
            }
        }

        auto upscaleAndBlend = [this](Texture2D* src, Texture2D* blend, Texture2D* dst) {
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

        CachedTexture2D upscalePyramid[numPasses + 1] = { };
        upscalePyramid[numPasses] = downsamplePyramid[numPasses];
        // upscale passes
        {
            for (i32 pass = numPasses; pass >= 1; --pass)
            { 
                Texture2D* src = upscalePyramid[pass].get();
                Texture2D* blend = downsamplePyramid[pass - 1].get();
                std::string passName("BloomUpscale");
                passName += '[' + std::to_string(pass) + ']';
                Texture2D::Spec spec = src->getSpec();
                spec.width *= 2;
                spec.height *= 2;
                // todo: need to decouple Sampler from texture!!!
                // todo: this is a bug here, I shouldn't use point sampling here
                upscalePyramid[pass - 1] = CachedTexture2D(passName.c_str(), spec);
                Texture2D* dst = upscalePyramid[pass - 1].get();

                upscaleAndBlend(src, blend, dst);
                gaussianBlur(dst, pass * 2, 1.f);
            }
        }
        return upscalePyramid[0];
    }

    void Renderer::compose(Texture2D* composited, Texture2D* inSceneColor, Texture2D* inBloomColor, const glm::uvec2& outputResolution) 
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

    void Renderer::gaussianBlur(Texture2D* inoutTexture, u32 inRadius, f32 inSigma) 
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
        Texture2D::Spec spec = inoutTexture->getSpec();
        name += '_' + std::to_string(spec.width) + 'x' + std::to_string(spec.height);
        CachedTexture2D scratchBuffer(name.c_str(), spec);
        auto scratch = scratchBuffer.get();
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

    // todo: refactor this, this should really just be renderScene()
    void Renderer::renderSceneToLightProbe(Scene* scene, LightProbe* probe, RenderTarget* renderTarget) {
#if 0 
        // updateTransforms(scene);

        // only capture static objects
        std::vector<Entity*> staticObjects;
        for (u32 i = 0; i < scene->entities.size(); ++i)
        {
            if (scene->entities[i]->m_static)
            {
                staticObjects.push_back(scene->entities[i]);
            }
        }

        // render sun light shadow
        renderSunShadow(scene, staticObjects);

        // turn off ssao(
        m_settings.bSSAOEnabled = false;

        // render scene into each face of the cubemap
        Camera camera = { };
        camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
        camera.n = 0.1f;
        camera.f = 100.f;
        camera.fov = glm::radians(90.f);

        for (u32 f = 0; f < (sizeof(LightProbeCameras::cameraFacingDirections)/sizeof(LightProbeCameras::cameraFacingDirections[0])); ++f)
        {
            camera.position = probe->position;
            camera.lookAt = camera.position + LightProbeCameras::cameraFacingDirections[f];
            camera.worldUp = LightProbeCameras::worldUps[f];
            camera.update();

            // updateFrameGlobalData(scene, camera);
            // draw skybox
            if (scene->m_skybox)
            {
                scene->m_skybox->render();
            }
            renderTarget->clear({ { (i32)f } });
            // draw entities 
            for (u32 i = 0; i < staticObjects.size(); ++i)
            {
                drawEntity(
                    renderTarget,
                    { { (i32)f}, {}, {}, {} },
                    false,
                    { 0u, 0u, probe->sceneCapture->width, probe->sceneCapture->height }, 
                    GfxPipelineState(), 
                    staticObjects[i]);
            }
        }

        m_settings.bSSAOEnabled = true;
#endif
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
        Texture2D::Spec spec(320, 180, 1, PF_RGB16F);
        static Texture2D* outTexture = createRenderTexture("CubemapViewerTexture", spec, Sampler2D());
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

                ImGui::Image((ImTextureID)outTexture->getGpuObject(), ImVec2(320, 180), ImVec2(0, 1), ImVec2(1, 0));

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
        Texture2D::Spec spec(320, 180, 1, PF_RGB16F);
        static Texture2D* outTexture = createRenderTexture("CubemapViewerTexture", spec, Sampler2D());
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

                ImGui::Image((ImTextureID)outTexture->getGpuObject(), ImVec2(320, 180), ImVec2(0, 1), ImVec2(1, 0));

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