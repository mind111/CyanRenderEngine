
#include "glfw3.h"
#include "gtx/quaternion.hpp"
#include "glm/gtc/integer.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"

#include "InstantRadiosity.h"
#include "CyanRenderer.h"
#include "RenderableScene.h"
#include "AssetManager.h"
#include "LightRenderable.h"

namespace Cyan {

    InstantRadiosity::InstantRadiosity() {

    }

    void InstantRadiosity::initialize() {
        glCreateBuffers(1, &VPLBuffer);
        glNamedBufferData(VPLBuffer, kMaxNumVPLs * sizeof(VPL), nullptr, GL_DYNAMIC_COPY);
        glCreateBuffers(1, &VPLCounter);
        glNamedBufferData(VPLCounter, sizeof(u32), nullptr, GL_DYNAMIC_COPY);
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

            ITextureRenderable::Spec spec = { };
            spec.type = TEX_2D;
            spec.width = kOctShadowMapResolution;
            spec.height = kOctShadowMapResolution;
            spec.pixelFormat = PF_RGB16F;

            ITextureRenderable::Parameter params = { };
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
        glCreateBuffers(1, &VPLShadowHandleBuffer);
        glNamedBufferData(VPLShadowHandleBuffer, sizeof(VPLShadowHandles), VPLShadowHandles, GL_DYNAMIC_COPY);
    }

    void InstantRadiosity::generateVPLs(Renderer* renderer, RenderableScene& renderableScene, const glm::uvec2& renderResolution) {
        ITextureRenderable::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = renderResolution.x;
        spec.height = renderResolution.y;
        spec.pixelFormat = PF_RGB16F;
        RenderTexture2D* outColor = renderer->m_renderQueue.createTexture2D("GenerateVPLOutput", spec);

        renderer->m_renderQueue.addPass(
            "GenerateVPLPass",
            [outColor](RenderQueue& renderQueue, RenderPass* pass) {
                pass->addInput(outColor);
                pass->addOutput(outColor);
            },
            [this, renderer, outColor, renderableScene]() mutable {
                std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(outColor->spec.width, outColor->spec.height));
                renderTarget->setColorBuffer(outColor->getTextureResource(), 0);
                renderTarget->clear({ { 0 } });

                auto gfxc = renderer->getGfxCtx();
                renderableScene.submitSceneData(gfxc);

                Shader* VPLGenerationShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "VPLGenerationShader", SHADER_SOURCE_PATH "generate_vpl_v.glsl", SHADER_SOURCE_PATH "generate_vpl_p.glsl" });
                gfxc->setShader(VPLGenerationShader);
                gfxc->setRenderTarget(renderTarget.get(), { { 0 } });
                gfxc->setViewport({ 0, 0, renderTarget->width, renderTarget->height });
                gfxc->setDepthControl(DepthControl::kEnable);

                for (auto light : renderableScene.lights)
                {
                    light->setShaderParameters(VPLGenerationShader);
                }
                VPLGenerationShader->setUniform("kMaxNumVPLs", kMaxNumVPLs);
                VPLGenerationShader->commit(gfxc);

                // bind VPL atomic counter
                numGeneratedVPLs = 0;
                glNamedBufferSubData(VPLCounter, 0, sizeof(u32), &numGeneratedVPLs);
                glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, VPLCounter);
                memset(VPLs, 0, sizeof(VPLs));
                // bind VPL buffer for gpu to write
                glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 47, VPLBuffer, 0, sizeof(VPLs));

                renderer->submitSceneMultiDrawIndirect(renderableScene);

                // read back VPL data
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);
                glGetNamedBufferSubData(VPLBuffer, 0, sizeof(VPL) * kMaxNumVPLs, VPLs);
                glGetNamedBufferSubData(VPLCounter, 0, sizeof(u32), &numGeneratedVPLs);
                numGeneratedVPLs = min(kMaxNumVPLs, numGeneratedVPLs);
            });
    }

    void InstantRadiosity::buildVPLShadowMaps(Renderer* renderer, RenderableScene& renderableScene) {
        // render VPL shadow maps
        auto renderVPLShadow = [this, renderer](i32 VPLIndex, RenderableScene& renderableScene, RenderTarget* depthRenderTarget) {
            glDisable(GL_CULL_FACE);
            glm::vec3 position = vec4ToVec3(VPLs[VPLIndex].position);
            // render point shadow map
            for (i32 pass = 0; pass < 6; ++pass) {
                glBindFramebuffer(GL_FRAMEBUFFER, depthRenderTarget->fbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + pass, VPLShadowCubemaps[VPLIndex], 0);
                depthRenderTarget->clear({ { 0 } });

                // update view matrix
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
                    renderer->submitMesh(
                        depthRenderTarget,
                        { },
                        false,
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
            renderer->submitFullScreenPass(
                octMappingRenderTarget.get(),
                { { 0 } },
                octMappingShader,
                [this, VPLIndex](RenderTarget* renderTarget, Shader* shader) {
                    shader->setUniform("srcCubemap", (i32)100);
                    glBindTextureUnit(100, VPLShadowCubemaps[VPLIndex]);
                }
            );
            glEnable(GL_CULL_FACE);
        };

        renderer->m_renderQueue.addPass(
            "BuildVPLShadowPass",
            [](RenderQueue& renderQueue, RenderPass* pass) {

            },
            [this, &renderableScene, renderVPLShadow]() {
                auto depthRenderTarget = std::unique_ptr<RenderTarget>(createDepthOnlyRenderTarget(kVPLShadowResolution, kVPLShadowResolution));
                for (i32 i = 0; i < numGeneratedVPLs; ++i) {
                    renderVPLShadow(i, renderableScene, depthRenderTarget.get());
                }
            }
        );
    }

    void InstantRadiosity::renderInternal(Renderer* renderer, RenderableScene& renderableScene, RenderTexture2D* output) {
        // render scene depth normal pass first
        auto depthPrepassOutput = renderer->renderSceneDepthNormal(renderableScene, glm::uvec2(1280u, 720u));
        auto sceneDepthBuffer = depthPrepassOutput.depthTexture;
        auto sceneNormalBuffer = depthPrepassOutput.normalTexture;

        // render radiosity 
        static i32 activeVPLs = 1;
        renderer->m_renderQueue.addPass(
            "InstantRadiosityPass",
            [sceneDepthBuffer, sceneNormalBuffer, output](RenderQueue& renderQueue, RenderPass* pass) {
                pass->addInput(sceneDepthBuffer);
                pass->addInput(sceneNormalBuffer);
                pass->addInput(output);
                pass->addOutput(output);
            },
            [renderer, sceneDepthBuffer, sceneNormalBuffer, this, output]() {
                auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(1280, 720));
                renderTarget->setColorBuffer(output->getTextureResource(), 0);
                Shader* shader = ShaderManager::createShader({ ShaderType::kVsPs, "InstantRadiosityShader", SHADER_SOURCE_PATH "instant_radiosity_v.glsl", SHADER_SOURCE_PATH "instant_radiosity_p.glsl" });

                // final blit to default framebuffer
                renderer->submitFullScreenPass(
                    renderTarget.get(),
                    { { 0u } },
                    shader,
                    [this, sceneDepthBuffer, sceneNormalBuffer](RenderTarget* renderTarget, Shader* shader) {
                        shader->setTexture("sceneDepthBuffer", sceneDepthBuffer->getTextureResource());
                        shader->setTexture("sceneNormalBuffer", sceneNormalBuffer->getTextureResource());
                        shader->setUniform("numVPLs", activeVPLs);
                        shader->setUniform("outputSize", glm::vec2(1280, 720));
                        shader->setUniform("shadowCubemap", (i32)101);
                        shader->setUniform("farClippingPlane", farClippingPlane);
                        shader->setUniform("indirectVisibility", bIndirectVisibility ? 1.f : .0f);
                        glBindTextureUnit(101, VPLShadowCubemaps[0]);
                        for (i32 i = 0; i < kMaxNumVPLs; ++i) {
                            if (glIsTextureHandleResidentARB(VPLShadowHandles[i]) == GL_FALSE) {
                                glMakeTextureHandleResidentARB(VPLShadowHandles[i]);
                            }
                        }
                        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 50, VPLShadowHandleBuffer);
                    });
            }
        );

        renderer->addUIRenderCommand([this, renderer, output]() {
            renderer->appendToRenderingTab([this, output]() {
                ImGui::Image((ImTextureID)(output->getTextureResource()->getGpuResource()), ImVec2(320, 180), ImVec2(0, 1), ImVec2(1, 0));
                ImGui::SliderInt("active VPL count", &activeVPLs, 1, numGeneratedVPLs);
                ImGui::Checkbox("indirect visibility", &bIndirectVisibility);
                ImGui::Image((ImTextureID)(VPLOctShadowMaps[0]->getGpuResource()), ImVec2(180, 180), ImVec2(0, 1), ImVec2(1, 0));
                });
            });
    }

    void InstantRadiosity::renderInternal(Renderer* renderer, RenderableScene& renderableScene, RenderTexture2D* sceneDepthBuffer, RenderTexture2D* sceneNormalBuffer, RenderTexture2D* output) {
        // render radiosity 
        static i32 activeVPLs = 1;
        renderer->m_renderQueue.addPass(
            "InstantRadiosityPass",
            [sceneDepthBuffer, sceneNormalBuffer, output](RenderQueue& renderQueue, RenderPass* pass) {
                pass->addInput(sceneDepthBuffer);
                pass->addInput(sceneNormalBuffer);
                pass->addInput(output);
                pass->addOutput(output);
            },
            [renderer, sceneDepthBuffer, sceneNormalBuffer, this, output]() {
                auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(1280, 720));
                renderTarget->setColorBuffer(output->getTextureResource(), 0);
                Shader* shader = ShaderManager::createShader({ ShaderType::kVsPs, "InstantRadiosityShader", SHADER_SOURCE_PATH "instant_radiosity_v.glsl", SHADER_SOURCE_PATH "instant_radiosity_p.glsl" });

                // final blit to default framebuffer
                renderer->submitFullScreenPass(
                    renderTarget.get(),
                    { { 0u } },
                    shader,
                    [this, sceneDepthBuffer, sceneNormalBuffer](RenderTarget* renderTarget, Shader* shader) {
                        shader->setTexture("sceneDepthBuffer", sceneDepthBuffer->getTextureResource());
                        shader->setTexture("sceneNormalBuffer", sceneNormalBuffer->getTextureResource());
                        shader->setUniform("numVPLs", activeVPLs);
                        shader->setUniform("outputSize", glm::vec2(1280, 720));
                        shader->setUniform("shadowCubemap", (i32)101);
                        shader->setUniform("farClippingPlane", farClippingPlane);
                        shader->setUniform("indirectVisibility", bIndirectVisibility ? 1.f : .0f);
                        glBindTextureUnit(101, VPLShadowCubemaps[0]);
                        for (i32 i = 0; i < kMaxNumVPLs; ++i) {
                            if (glIsTextureHandleResidentARB(VPLShadowHandles[i]) == GL_FALSE) {
                                glMakeTextureHandleResidentARB(VPLShadowHandles[i]);
                            }
                        }
                        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 50, VPLShadowHandleBuffer);
                    });
            }
        );

        renderer->addUIRenderCommand([this, renderer, output]() {
            renderer->appendToRenderingTab([this]() {
                    ImGui::CollapsingHeader("Instant Radiosity", ImGuiTreeNodeFlags_DefaultOpen);
                    ImGui::Text("Num of VPLs: %u", kMaxNumVPLs);
                    if (ImGui::Button("Regenerate VPLs")) {
                        bRegenerateVPLs = true;
                    }
                    ImGui::Image((ImTextureID)(VPLOctShadowMaps[0]->getGpuResource()), ImVec2(180, 180), ImVec2(0, 1), ImVec2(1, 0));
            }); 

            renderer->appendToRenderingTab([this, output]() {
                ImGui::Image((ImTextureID)(output->getTextureResource()->getGpuResource()), ImVec2(320, 180), ImVec2(0, 1), ImVec2(1, 0));
                ImGui::SliderInt("active VPL count", &activeVPLs, 1, numGeneratedVPLs);
                ImGui::Checkbox("indirect visibility", &bIndirectVisibility);
                });
            });
    }
    
    void InstantRadiosity::render(Renderer* renderer, RenderableScene& renderableScene, RenderTexture2D* output) {
        const glm::vec2 res(2560, 1440);
        if (bRegenerateVPLs) {
            generateVPLs(renderer, renderableScene, res);
            buildVPLShadowMaps(renderer, renderableScene);
            bRegenerateVPLs = false;
        }
        renderInternal(renderer, renderableScene, output);
    }

    void InstantRadiosity::render(Renderer* renderer, RenderableScene& renderableScene, RenderTexture2D* sceneDepthBuffer, RenderTexture2D* sceneNormalBuffer, RenderTexture2D* output) {
        const glm::vec2 res(2560, 1440);
        if (bRegenerateVPLs) {
            generateVPLs(renderer, renderableScene, res);
            buildVPLShadowMaps(renderer, renderableScene);
            bRegenerateVPLs = false;
        }
        renderInternal(renderer, renderableScene, sceneDepthBuffer, sceneNormalBuffer, output);
    }

    void InstantRadiosity::visualizeVPLs(Renderer* renderer, RenderTarget* renderTarget, RenderableScene& renderableScene) {
        Shader* debugDrawShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "DebugDrawShader", SHADER_SOURCE_PATH "debug_draw_v.glsl", SHADER_SOURCE_PATH "debug_draw_p.glsl" });
        // debug draw VPLs
        for (i32 i = 0; i < kMaxNumVPLs; ++i) {
            renderer->debugDrawSphere(
                renderTarget,
                { { 0 } },
                { 0, 0, renderTarget->width, renderTarget->height },
                vec4ToVec3(VPLs[i].position),
                glm::vec3(0.02),
                renderableScene.camera.view,
                renderableScene.camera.projection
            );
        }
    }
}
