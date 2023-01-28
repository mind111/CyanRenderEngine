
#include <glfw/glfw3.h>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/integer.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "InstantRadiosity.h"
#include "CyanRenderer.h"
#include "RenderableScene.h"
#include "AssetManager.h"
#include "Lights.h"

namespace Cyan {

    InstantRadiosity::InstantRadiosity() {

    }

    void InstantRadiosity::initialize() {
        glCreateBuffers(1, &VPLBuffer);
        glNamedBufferData(VPLBuffer, kMaxNumVPLs * sizeof(VPL), nullptr, GL_DYNAMIC_COPY);
        glCreateBuffers(1, &VPLCounter);
        glNamedBufferData(VPLCounter, sizeof(u32), nullptr, GL_DYNAMIC_COPY);

        // initialize basic cubemap shadow map
        glCreateTextures(GL_TEXTURE_CUBE_MAP, kMaxNumVPLs, VPLShadowCubemaps);
        for (i32 i = 0; i < kMaxNumVPLs; ++i) {
            glBindTexture(GL_TEXTURE_CUBE_MAP, VPLShadowCubemaps[i]);
            for (i32 i = 0; i < 6; ++i) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, kVPLShadowResolution, kVPLShadowResolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

            ITexture::Spec spec = { };
            spec.type = TEX_2D;
            spec.width = kOctShadowMapResolution;
            spec.height = kOctShadowMapResolution;
            spec.pixelFormat = PF_RGB16F;

            ITexture::Parameter params = { };
            params.minificationFilter = FM_POINT;
            params.magnificationFilter = FM_POINT;
            params.wrap_r = WM_CLAMP;
            params.wrap_s = WM_CLAMP;
            params.wrap_t = WM_CLAMP;

            char name[64] = { };
            sprintf_s(name, "VPLOctShadowMap_%d", i);
            VPLOctShadowMaps[i] = AssetManager::createTexture2D(name, spec, params);
#if BINDLESS_TEXTURE
            VPLShadowHandles[i] = VPLOctShadowMaps[i]->glHandle;
#endif
        }

        // initiailize VSM shadow map
        glCreateTextures(GL_TEXTURE_CUBE_MAP, kMaxNumVPLs, VPLVSMs);
        for (i32 i = 0; i < kMaxNumVPLs; ++i) {
            glBindTexture(GL_TEXTURE_CUBE_MAP, VPLVSMs[i]);
            for (i32 i = 0; i < 6; ++i) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RG16F, kVPLShadowResolution, kVPLShadowResolution, 0, GL_RG, GL_FLOAT, nullptr);
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

            ITexture::Spec spec = { };
            spec.type = TEX_2D;
            spec.width = kOctShadowMapResolution;
            spec.height = kOctShadowMapResolution;
            spec.pixelFormat = PF_RGB16F;

            ITexture::Parameter params = { };
            params.minificationFilter = FM_POINT;
            params.magnificationFilter = FM_POINT;
            params.wrap_r = WM_CLAMP;
            params.wrap_s = WM_CLAMP;
            params.wrap_t = WM_CLAMP;

            char name[64] = { };
            sprintf_s(name, "VPLOctShadowMap_%d", i);
            VPLOctVSMs[i] = AssetManager::createTexture2D(name, spec, params);
#if BINDLESS_TEXTURE
            VPLVSMHandles[i] = VPLOctVSMs[i]->glHandle;
#endif
        }
        glCreateBuffers(1, &VPLShadowHandleBuffer);
        glNamedBufferData(VPLShadowHandleBuffer, sizeof(VPLShadowHandles), VPLShadowHandles, GL_DYNAMIC_COPY);
    }

    void InstantRadiosity::generateVPLs(Renderer* renderer, RenderableScene& renderableScene, const glm::uvec2& renderResolution) {
        ITexture::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = renderResolution.x;
        spec.height = renderResolution.y;
        spec.pixelFormat = PF_RGB16F;
        static Texture2D* outColor = new Texture2D("GenerateVPLOutput", spec);

        std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(outColor->width, outColor->height));
        renderTarget->setColorBuffer(outColor, 0);
        renderTarget->clear({ { 0 } });

        auto gfxc = renderer->getGfxCtx();
        renderableScene.upload();

        VertexShader* vs = ShaderManager::createShader<VertexShader>("VPLGenerationVS", SHADER_SOURCE_PATH "generate_vpl_v.glsl");
        PixelShader* ps = ShaderManager::createShader<PixelShader>("VPLGenerationPS", SHADER_SOURCE_PATH "generate_vpl_p.glsl");
        PixelPipeline* VPLGenerationPipeline = ShaderManager::createPixelPipeline("VPLGeneration", vs, ps);

        gfxc->setPixelPipeline(VPLGenerationPipeline, [](VertexShader* vs, PixelShader* ps) {
            // todo: having to know which shader owns the uniform is kind of inconvenient
            ps->setUniform("kMaxNumVPLs", kMaxNumVPLs);
        });

        gfxc->setRenderTarget(renderTarget.get(), { { 0 } });
        gfxc->setViewport({ 0, 0, renderTarget->width, renderTarget->height });
        gfxc->setDepthControl(DepthControl::kEnable);

        // bind VPL atomic counter
        numGeneratedVPLs = 0;
        glNamedBufferSubData(VPLCounter, 0, sizeof(u32), &numGeneratedVPLs);
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, VPLCounter);
        memset(VPLs, 0, sizeof(VPLs));
        // bind VPL buffer for gpu to write
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 47, VPLBuffer, 0, sizeof(VPLs));

        renderer->multiDrawSceneIndirect(renderableScene);

        // read back VPL data
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);
        glGetNamedBufferSubData(VPLBuffer, 0, sizeof(VPL) * kMaxNumVPLs, VPLs);
        glGetNamedBufferSubData(VPLCounter, 0, sizeof(u32), &numGeneratedVPLs);
        numGeneratedVPLs = min(kMaxNumVPLs, numGeneratedVPLs);
    }

    void InstantRadiosity::buildVPLVSMs(Renderer* renderer, RenderableScene& renderableScene) {
#if 0
        auto renderVPLVSM = [this, renderer](i32 VPLIndex, RenderableScene& renderableScene, RenderTarget* renderTarget) {
            glDisable(GL_CULL_FACE);
            glm::vec3 position = vec4ToVec3(VPLs[VPLIndex].position);
            // render point shadow map
            for (i32 pass = 0; pass < 6; ++pass) {
                glBindFramebuffer(GL_FRAMEBUFFER, renderTarget->fbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + pass, GL_TEXTURE_CUBE_MAP_POSITIVE_X + pass, VPLVSMs[VPLIndex], 0);
                renderTarget->clear({ { pass, glm::vec4(1.f) } });

                // upload view matrix
                glm::vec3 lookAt = position + LightProbeCameras::cameraFacingDirections[pass];
                PerspectiveCamera camera(
                    position,
                    lookAt,
                    LightProbeCameras::worldUps[pass],
                    fov,
                    nearClippingPlane,
                    farClippingPlane,
                    aspectRatio
                );
                glm::mat4 view = camera.view();
                glm::mat4 projection = camera.projection();

                VertexShader* vs = ShaderManager::createShader<VertexShader>("PointVSMShadowVS", SHADER_SOURCE_PATH "point_shadow_v.glsl");
                PixelShader* ps = ShaderManager::createShader<PixelShader>("PointVSMShadowPS", SHADER_SOURCE_PATH "point_shadow_p.glsl");
                PixelPipeline* pipeline = ShaderManager::createPixelPipeline("PointVSMShadow", vs, ps);

                // render mesh instances
                i32 transformIndex = 0u;
                for (auto meshInst : renderableScene.meshInstances) {
                    renderer->drawMesh(
                        renderTarget,
                        { 0, 0, renderTarget->width, renderTarget->height},
                        GfxPipelineState(),
                        meshInst->parent,
                        pointShadowShader,
                        [this, VPLIndex, pass, &transformIndex, view, projection, position](RenderTarget* renderTarget, Shader* shader) {
                            shader->setUniform("transformIndex", transformIndex);
                            shader->setUniform("view", view);
                            shader->setUniform("projection", projection);
                            shader->setUniform("farClippingPlane", farClippingPlane);
                            shader->setUniform("lightPosition", position);
                        }
                    );
                    transformIndex++;
                }
            }

            // project the cubemap into a quad using octahedral mapping
            auto octMappingRenderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(VPLOctVSMs[VPLIndex]->width, VPLOctVSMs[VPLIndex]->height));
            octMappingRenderTarget->setColorBuffer(VPLOctVSMs[VPLIndex], 0);
            octMappingRenderTarget->clear({ { 0, glm::vec4(1.f) } });
            Shader* octMappingShader = ShaderManager::createShader({ ShaderType::kVsPs, "OctMappingShader", SHADER_SOURCE_PATH "oct_mapping_v.glsl", SHADER_SOURCE_PATH "oct_mapping_p.glsl" });
            renderer->drawFullscreenQuad(
                octMappingRenderTarget.get(),
                octMappingShader,
                [this, VPLIndex](RenderTarget* renderTarget, Shader* shader) {
                    shader->setUniform("srcCubemap", (i32)100);
                    glBindTextureUnit(100, VPLVSMs[VPLIndex]);
                }
            );
            glEnable(GL_CULL_FACE);
        };

        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(kVPLShadowResolution, kVPLShadowResolution));
        for (i32 i = 0; i < numGeneratedVPLs; ++i) {
            renderVPLVSM(i, renderableScene, renderTarget.get());
        }

        // todo: caught a bug when trying to use the following code. because the generateVPL pass' execution is deferred, when we 
        // need to determine how many shadow maps that we need to fitler, numGeneratedVPLs is still 0 here because the code for reading back
        // the value from gpu is not executed yet
        // todo: another issue, should we allow adding render passes within addPass(), my first thought is that we shouldn't allow that
        /*
        for (i32 i = 0; i < numGeneratedVPLs; ++i) {
            // filter the depth map for vsm
            auto vsm = renderer->m_renderQueue.registerTexture2D(VPLOctVSMs[i]);
            renderer->gaussianBlurInPlace(vsm, 3.f, 2.f);
            renderer->gaussianBlurInPlace(vsm, 5.f, 3.f);
        }
        */
#if 0
        for (i32 i = 0; i < numGeneratedVPLs; ++i) {
            for (i32 j = 0; j < 5; ++j) {
                renderer->gaussianBlurImmediate(VPLOctVSMs[i], 5.f, 3.f);
            }
        }
#endif
#endif
    }

    void InstantRadiosity::buildVPLShadowMaps(Renderer* renderer, RenderableScene& renderableScene) {
#if 0
        // render VPL basic shadow maps
        auto renderVPLShadow = [this, renderer](i32 VPLIndex, RenderableScene& renderableScene, RenderTarget* depthRenderTarget) {
            glDisable(GL_CULL_FACE);
            glm::vec3 position = vec4ToVec3(VPLs[VPLIndex].position);
            // render point shadow map
            for (i32 pass = 0; pass < 6; ++pass) {
                glBindFramebuffer(GL_FRAMEBUFFER, depthRenderTarget->fbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + pass, VPLShadowCubemaps[VPLIndex], 0);
                depthRenderTarget->clear({ { pass } });

                // upload view matrix
                glm::vec3 lookAt = position + LightProbeCameras::cameraFacingDirections[pass];
                PerspectiveCamera camera(
                    position,
                    lookAt,
                    LightProbeCameras::worldUps[pass],
                    fov,
                    nearClippingPlane,
                    farClippingPlane,
                    aspectRatio
                );
                glm::mat4 view = camera.view();
                glm::mat4 projection = camera.projection();

                Shader* pointShadowShader = ShaderManager::createShader({ ShaderType::kVsPs, "PointShadowShader", SHADER_SOURCE_PATH "point_shadow_v.glsl", SHADER_SOURCE_PATH "point_shadow_p.glsl" });

                // render mesh instances
                i32 transformIndex = 0u;
                for (auto meshInst : renderableScene.meshInstances)
                {
                    renderer->drawMesh(
                        depthRenderTarget,
                        { 0, 0, depthRenderTarget->width, depthRenderTarget->height},
                        GfxPipelineState(),
                        meshInst->parent,
                        pointShadowShader,
                        [this, VPLIndex, pass, &transformIndex, view, projection, position](RenderTarget* renderTarget, Shader* shader) {
                            shader->setUniform("transformIndex", transformIndex);
                            shader->setUniform("view", view);
                            shader->setUniform("projection", projection);
                            shader->setUniform("farClippingPlane", farClippingPlane);
                            shader->setUniform("lightPosition", position);
                        }
                    );
                    transformIndex++;
                }
            }

            // project the cubemap into a quad using octahedral mapping
            auto octMappingRenderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(VPLOctShadowMaps[VPLIndex]->width, VPLOctShadowMaps[VPLIndex]->height));
            octMappingRenderTarget->setColorBuffer(VPLOctShadowMaps[VPLIndex], 0);
            Shader* octMappingShader = ShaderManager::createShader({ ShaderType::kVsPs, "OctMappingShader", SHADER_SOURCE_PATH "oct_mapping_v.glsl", SHADER_SOURCE_PATH "oct_mapping_p.glsl" });
            renderer->drawFullscreenQuad(
                octMappingRenderTarget.get(),
                octMappingShader,
                [this, VPLIndex](RenderTarget* renderTarget, Shader* shader) {
                    shader->setUniform("srcCubemap", (i32)100);
                    glBindTextureUnit(100, VPLShadowCubemaps[VPLIndex]);
                }
            );
            glEnable(GL_CULL_FACE);
        };

        auto depthRenderTarget = std::unique_ptr<RenderTarget>(createDepthOnlyRenderTarget(kVPLShadowResolution, kVPLShadowResolution));
        for (i32 i = 0; i < numGeneratedVPLs; ++i) {
            renderVPLShadow(i, renderableScene, depthRenderTarget.get());
        }
#endif
    }

    void InstantRadiosity::renderInternal(Renderer* renderer, RenderableScene& renderableScene, Texture2D* output) {
        // render scene depth normal pass first
        ITexture::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = 1280;
        spec.height = 720;
        spec.pixelFormat = PF_RGB32F;
        static Texture2D* sceneDepthBuffer = new Texture2D("IRSceneDepthBuffer", spec);
        static Texture2D* sceneNormalBuffer = new Texture2D("IRSceneNormalBuffer", spec);
        std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(spec.width, spec.height));

        renderer->renderSceneDepthNormal(renderableScene, renderTarget.get(), sceneDepthBuffer, sceneNormalBuffer);
        renderInternal(renderer, renderableScene, sceneDepthBuffer, sceneNormalBuffer, output);
    }

    void InstantRadiosity::renderInternal(Renderer* renderer, RenderableScene& renderableScene, Texture2D* sceneDepthBuffer, Texture2D* sceneNormalBuffer, Texture2D* output) {
#if 0
        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(1280, 720));
        renderTarget->setColorBuffer(output, 0);
        Shader* shader = ShaderManager::createShader({ ShaderType::kVsPs, "InstantRadiosityShader", SHADER_SOURCE_PATH "instant_radiosity_v.glsl", SHADER_SOURCE_PATH "instant_radiosity_p.glsl" });

        // final blit to default framebuffer
        renderer->drawFullscreenQuad(
            renderTarget.get(),
            shader,
            [this, sceneDepthBuffer, sceneNormalBuffer](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("sceneDepthBuffer", sceneDepthBuffer);
                shader->setTexture("sceneNormalBuffer", sceneNormalBuffer);
                shader->setUniform("numVPLs", activeVPLs);
                shader->setUniform("outputSize", glm::vec2(1280, 720));
                shader->setUniform("farClippingPlane", farClippingPlane);
                shader->setUniform("indirectVisibility", bIndirectVisibility ? 1.f : .0f);
                shader->setUniform("shadowAlgorithm", (i32)m_shadowAlgorithm);

                // bind shadow map texture handles
                switch (m_shadowAlgorithm) {
                case VPLShadowAlgorithm::kBasic: {
                    for (i32 i = 0; i < kMaxNumVPLs; ++i) {
                        if (glIsTextureHandleResidentARB(VPLShadowHandles[i]) == GL_FALSE) {
                            glMakeTextureHandleResidentARB(VPLShadowHandles[i]);
                        }
                    }
                    glNamedBufferSubData(VPLShadowHandleBuffer, 0, sizeof(VPLShadowHandles), VPLShadowHandles);
                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 50, VPLShadowHandleBuffer);
                } break;
                case VPLShadowAlgorithm::kVSM: {
                    for (i32 i = 0; i < kMaxNumVPLs; ++i) {
                        if (glIsTextureHandleResidentARB(VPLVSMHandles[i]) == GL_FALSE) {
                            glMakeTextureHandleResidentARB(VPLVSMHandles[i]);
                        }
                    }
                    glNamedBufferSubData(VPLShadowHandleBuffer, 0, sizeof(VPLVSMHandles), VPLVSMHandles);
                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 50, VPLShadowHandleBuffer);
                } break;
                default:
                    break;
                };
            });
#endif
    }
    
    Texture2D* InstantRadiosity::render(Renderer* renderer, RenderableScene& renderableScene, const glm::uvec2& renderResolution) {
        ITexture::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = renderResolution.x;
        spec.height = renderResolution.y;
        spec.pixelFormat = PF_RGB16F;
        static Texture2D* radiosity = new Texture2D("InstantRadiosity", spec);

        const glm::vec2 res(2560, 1440);
        if (bRegenerateVPLs) {
            generateVPLs(renderer, renderableScene, res);
            switch (m_shadowAlgorithm) {
            case VPLShadowAlgorithm::kBasic:
                buildVPLShadowMaps(renderer, renderableScene);
                break;
            case VPLShadowAlgorithm::kVSM:
                buildVPLVSMs(renderer, renderableScene);
                break;
            default:
                break;
            }
            bRegenerateVPLs = false;
        }

        renderInternal(renderer, renderableScene, radiosity);
        renderUI(renderer, radiosity);
        
        return radiosity;
    }

     Texture2D* InstantRadiosity::render(Renderer* renderer, RenderableScene& renderableScene, Texture2D* sceneDepthBuffer, Texture2D* sceneNormalBuffer, const glm::uvec2& renderResolution) {
        ITexture::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = renderResolution.x;
        spec.height = renderResolution.y;
        spec.pixelFormat = PF_RGB16F;
        static Texture2D* radiosity = new Texture2D("InstantRadiosity", spec);

        const glm::vec2 res(2560, 1440);
        if (bRegenerateVPLs) {
            generateVPLs(renderer, renderableScene, res);
            switch (m_shadowAlgorithm) {
            case VPLShadowAlgorithm::kBasic:
                buildVPLShadowMaps(renderer, renderableScene);
                break;
            case VPLShadowAlgorithm::kVSM:
                buildVPLVSMs(renderer, renderableScene);
                break;
            default:
                break;
            }
            bRegenerateVPLs = false;
        }

        renderInternal(renderer, renderableScene, sceneDepthBuffer, sceneNormalBuffer, radiosity);
        renderUI(renderer, radiosity);

        return radiosity;
    }

    void InstantRadiosity::visualizeVPLs(Renderer* renderer, RenderTarget* renderTarget, RenderableScene& renderableScene) {
#if 0
        Shader* debugDrawShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "DebugDrawShader", SHADER_SOURCE_PATH "debug_draw_v.glsl", SHADER_SOURCE_PATH "debug_draw_p.glsl" });
        // debug draw VPLs
        for (i32 i = 0; i < kMaxNumVPLs; ++i) {
            renderer->debugDrawSphere(
                renderTarget,
                { 0, 0, renderTarget->width, renderTarget->height },
                vec4ToVec3(VPLs[i].position),
                glm::vec3(0.02),
                renderableScene.camera.view,
                renderableScene.camera.projection
            );
        }
#endif
    }

    void InstantRadiosity::renderUI(Renderer* renderer, Texture2D* output) {
#if 0
        renderer->addUIRenderCommand([this, renderer, output]() {
            renderer->appendToRenderingTab([this, output]() {
                ImGui::CollapsingHeader("Instant Radiosity", ImGuiTreeNodeFlags_DefaultOpen);
                if (ImGui::Button("regenerate VPLs")) {
                    bRegenerateVPLs = true;
                }
                static i32 shadowAlgo = (i32)m_shadowAlgorithm;
                const char* shadowAlgoNames[(i32)VPLShadowAlgorithm::kCount] = { "Basic", "VSM" };
                ImVec2 imageSize(320, 180);
                ImGui::Combo("VPL shadow algorithm", &shadowAlgo, shadowAlgoNames, (i32)VPLShadowAlgorithm::kCount);
                m_shadowAlgorithm = (VPLShadowAlgorithm)(shadowAlgo);
                ImGui::Image((ImTextureID)(output->getGpuObject()), ImVec2(320, 180), ImVec2(0, 1), ImVec2(1, 0));
                ImGui::SliderInt("active VPL count", &activeVPLs, 1, numGeneratedVPLs);
                ImGui::Checkbox("indirect visibility", &bIndirectVisibility);
                ImGui::Text("VPL oct shadow");
                switch (m_shadowAlgorithm) {
                case VPLShadowAlgorithm::kBasic:
                    ImGui::Image((ImTextureID)(VPLOctShadowMaps[0]->getGpuObject()), ImVec2(180, 180), ImVec2(0, 1), ImVec2(1, 0));
                    break;
                case VPLShadowAlgorithm::kVSM:
                    ImGui::Image((ImTextureID)(VPLOctVSMs[0]->getGpuObject()), ImVec2(180, 180), ImVec2(0, 1), ImVec2(1, 0));
                    break;
                default:
                    break;
                };
                });
            });
#endif
    }
}
