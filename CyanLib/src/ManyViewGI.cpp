
#include "ManyViewGI.h"
#include "CyanRenderer.h"
#include "LightRenderable.h"
#include "AssetManager.h"

namespace Cyan {

    glm::mat4 calcHemicubeTangentFrame(const ManyViewGI::Hemicube& hemicube) {
        return calcTangentFrame(hemicube.normal);
    }

    glm::mat4 calcHemicubeTransform(const ManyViewGI::Hemicube& hemicube, const glm::vec3& scale) {
        glm::mat4 tangentFrame = calcHemicubeTangentFrame(hemicube);
        glm::mat4 transform = glm::translate(glm::mat4(hemicube.position.w), vec4ToVec3(hemicube.position));
        transform = transform * tangentFrame;
        transform = glm::scale(transform, scale);
        return transform;
    }

    static void drawSurfels(ShaderStorageBuffer<DynamicSsboData<InstanceDesc>>& surfelInstanceBuffer, Renderer* renderer, GfxContext* gfxc, RenderTarget* dstRenderTarget) {
        surfelInstanceBuffer.bind(68);
        auto disk = AssetManager::getAsset<Mesh>("Disk");
        auto shader = ShaderManager::createShader({ 
            ShaderSource::Type::kVsPs,
            "VisualizeSurfelShader",
            SHADER_SOURCE_PATH "visualize_surfel_v.glsl",
            SHADER_SOURCE_PATH "visualize_surfel_p.glsl"
            });
        gfxc->setRenderTarget(dstRenderTarget);
        gfxc->setViewport({ 0, 0, dstRenderTarget->width, dstRenderTarget->height });
        gfxc->setShader(shader);
        auto va = disk->getSubmesh(0)->getVertexArray();
        gfxc->setVertexArray(va);
        u32 numInstances = surfelInstanceBuffer.getNumElements();
        if (va->hasIndexBuffer()) {
            glDrawElementsInstanced(GL_TRIANGLES, disk->getSubmesh(0)->numIndices(), GL_UNSIGNED_INT, 0, numInstances);
        }
        else {
            glDrawArraysInstanced(GL_TRIANGLES, 0, disk->getSubmesh(0)->numVertices(), numInstances);
        }
    }

    ManyViewGI::ManyViewGI(Renderer* renderer, GfxContext* gfxc) 
        : m_renderer(renderer), m_gfxc(gfxc) {

    }

    void ManyViewGI::initialize() {
        if (!bInitialized) {
            maxNumHemicubes = irradianceAtlasRes.x * irradianceAtlasRes.y;
            hemicubes.resize(maxNumHemicubes);
            radianceCubemaps.resize(maxNumHemicubes);
            jitteredSampleDirections.resize(maxNumHemicubes);
            for (i32 i = 0; i < maxNumHemicubes; ++i) {
                std::string name = "RadianceCubemap_" + std::to_string(i);
                ITextureRenderable::Spec spec = {};
                spec.width = finalGatherRes;
                spec.height = finalGatherRes;
                spec.type = TEX_CUBE;
                spec.pixelFormat = PF_RGB16F;
                ITextureRenderable::Parameter params = {};
                /** note - @mind:
                * ran into a "incomplete texture" error when setting magnification filter to FM_TRILINEAR
                */
                params.minificationFilter = FM_BILINEAR;
                // todo: based on the rendering output, pay attention to the difference between using point/bilinear sampling
                params.magnificationFilter = FM_BILINEAR;
                params.wrap_r = WM_CLAMP;
                params.wrap_s = WM_CLAMP;
                params.wrap_t = WM_CLAMP;
                radianceCubemaps[i] = new TextureCubeRenderable(name.c_str(), spec, params);
            }

            glCreateBuffers(1, &hemicubeBuffer);
            u32 bufferSize = maxNumHemicubes * sizeof(Hemicube);
            glNamedBufferData(hemicubeBuffer, bufferSize, nullptr, GL_DYNAMIC_COPY);

            ITextureRenderable::Spec radianceAtlasSpec = { };
            radianceAtlasSpec.type = TEX_2D;
            radianceAtlasSpec.width = radianceAtlasRes.x;
            radianceAtlasSpec.height = radianceAtlasRes.y;
            radianceAtlasSpec.pixelFormat = PF_RGBA16F;
            radianceAtlas = new Texture2DRenderable("RadianceAtlas", radianceAtlasSpec);
            radianceAtlasRenderTarget = createRenderTarget(radianceAtlas->width, radianceAtlas->height);
            radianceAtlasRenderTarget->setColorBuffer(radianceAtlas, 0);

            /** note - @min:
            * it seems imageStore() only supports 1,2,4 channels textures, so using rgba16f here
            */
            ITextureRenderable::Spec irradianceAtlasSpec = { };
            irradianceAtlasSpec.type = TEX_2D;
            irradianceAtlasSpec.width = irradianceAtlasRes.x;
            irradianceAtlasSpec.height = irradianceAtlasRes.y;
            irradianceAtlasSpec.pixelFormat = PF_RGBA16F;
            irradianceAtlas = new Texture2DRenderable("IrradianceAtlas", irradianceAtlasSpec);

            ITextureRenderable::Spec spec = {};
            spec.type = TEX_2D;
            spec.width = visualizations.resolution.x;
            spec.height = visualizations.resolution.y;
            spec.pixelFormat = PF_RGB16F;
            visualizations.shared = new Texture2DRenderable("IrradianceVisualization", spec);

            // register visualizations
            m_renderer->registerVisualization("ManyViewGI", radianceAtlas);
            m_renderer->registerVisualization("ManyViewGI", irradianceAtlas);
            m_renderer->registerVisualization("ManyViewGI", visualizations.shared);

            bInitialized = true;
        }

        customInitialize();
    }

    // todo: 'scene' here really need to be passed in by value instead of by reference so that the ManyViewGI don't have to 
    // worry about any state changes to the scene that it's working on
    void ManyViewGI::setup(const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) {
        reset();
        bBuilding = true;
        // copy the scene data
        m_scene.reset(new SceneRenderable(scene));
        m_scene->submitSceneData(m_gfxc);

        // todo: clear irradiance atlas 

        placeHemicubes(depthBuffer, normalBuffer);

        customSetup(*m_scene, depthBuffer, normalBuffer);
    }

    void ManyViewGI::reset() {
        bBuilding = false;
        m_scene.reset(nullptr);
        renderedFrames = 0;
        // clear the radiance atlas
        glm::vec3 skyColor(0.529f, 0.808f, 0.922f);
        // radianceAtlasRenderTarget->clear({ { 0, glm::vec4(skyColor, 1.f) } });
        radianceAtlasRenderTarget->setDrawBuffers({ 0 });
        radianceAtlasRenderTarget->clearDrawBuffer({ 0 }, glm::vec4(0.2f, 0.2f, 0.2f, 1.f));
    }

    void ManyViewGI::render(RenderTarget* sceneRenderTarget, const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) {
        if (bBuilding) {
            progressiveBuildRadianceAtlas(*m_scene);
        }

        // debug visualization
        auto visRenderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(
            visualizations.shared->width, 
            visualizations.shared->height)
        );
        visRenderTarget->setColorBuffer(visualizations.shared, 0);
        visRenderTarget->setDrawBuffers({ 0 });
        visRenderTarget->clearDrawBuffer(0, glm::vec4(.2f, .2f, .2f, 1.f));
        if (opts.bVisualizeIrradiance) {
            visualizeIrradiance(visRenderTarget.get());
        }
        if (opts.bVisualizeHemicubes) {
            visualizeHemicubes(visRenderTarget.get());
        }
        customRender(sceneRenderTarget, visRenderTarget.get());

        // ui controls
        m_renderer->addUIRenderCommand([this, scene, depthBuffer, normalBuffer]() {
            ImGui::Begin("ManyView GI"); {
                if (ImGui::Button("Build Indirect Lighting")) {
                    setup(scene, depthBuffer, normalBuffer);
                }
                ImGui::Checkbox("Visualize Hemicubes", &opts.bVisualizeHemicubes);
                ImGui::Checkbox("Visualize Irradiance", &opts.bVisualizeIrradiance);

                customUI();
            } ImGui::End();
        });
    }

    void ManyViewGI::placeHemicubes(Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) {
        auto renderer = Renderer::get();
        // clear hemicube buffer
        u32 buffSize = sizeof(hemicubes[0]) * hemicubes.size();
        memset(hemicubes.data(), 0x0, buffSize);

        // todo: how to cache spawned raster cubes and avoid placing duplicated raster cubes
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 52, hemicubeBuffer);

        auto fillRasterShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "FillRasterShader", SHADER_SOURCE_PATH "blit_v.glsl", SHADER_SOURCE_PATH "fill_raster_p.glsl" });
        auto fillRasterRenderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(irradianceAtlasRes.x, irradianceAtlasRes.y));
        renderer->submitFullScreenPass(
            fillRasterRenderTarget.get(),
            fillRasterShader,
            [this, depthBuffer, normalBuffer](RenderTarget* renderTarget, Shader* shader) {
                shader->setUniform("outputSize", glm::vec2(renderTarget->width, renderTarget->height));
                shader->setUniform("maxNumRadianceCubes", maxNumHemicubes);
                shader->setTexture("depthBuffer", depthBuffer);
                shader->setTexture("normalBuffer", normalBuffer);
            }
        );

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // read back data from gpu
        glGetNamedBufferSubData(hemicubeBuffer, 0, buffSize, hemicubes.data());

        // generate jittered sample directions based on radiance cube's normal vector 
        for (i32 i = 0; i < maxNumHemicubes; ++i) {
            if (hemicubes[i].position.w > 0.f) {
                jitteredSampleDirections[i] = m_sampler.sample(hemicubes[i].normal);
            }
        }
    }

    void ManyViewGI::gatherRadiance(const Hemicube& radianceCube, u32 radianceCubeIndex, bool jitter, const SceneRenderable& scene, TextureCubeRenderable* radianceCubemap) {
        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(radianceCubemap->resolution, radianceCubemap->resolution));
        renderTarget->setColorBuffer(radianceCubemap, 0);
        m_gfxc->setRenderTarget(renderTarget.get());
        m_gfxc->setDepthControl(DepthControl::kEnable);
        m_gfxc->setViewport({ 0, 0, renderTarget->width, renderTarget->height });
        for (i32 face = 0; face < 6; ++face) {
            renderTarget->setDrawBuffers({ face });
            renderTarget->clearDrawBuffer(0, glm::vec4(0.f, 0.f, 0.f, 1.f));

            // skip the bottom face as it doesn't contribute to shared at all
            if (face == 3) {
                continue;
            }

            // calculate the tangent frame of radiance cube
            glm::vec3 worldUp = glm::vec3(0.f, 1.f, 0.f);
            glm::vec3 up = radianceCube.normal;
            if (jitter) {
                up = jitteredSampleDirections[radianceCubeIndex];
            }
            if (abs(up.y) > 0.98) {
                worldUp = glm::vec3(0.f, 0.f, -1.f);
            }
            glm::vec3 right = glm::cross(worldUp, up);
            glm::vec3 forward = glm::cross(up, right);
            glm::mat3 tangentFrame = {
                right,
                up,
                -forward
            };
            /** note - @min: 
            * the order of camera facing directions should match the following
                    {1.f, 0.f, 0.f},   // Right
                    {-1.f, 0.f, 0.f},  // Left
                    {0.f, 1.f, 0.f},   // Up
                    {0.f, -1.f, 0.f},  // Down
                    {0.f, 0.f, 1.f},   // Front
                    {0.f, 0.f, -1.f},  // Back
            */
            glm::vec3 cameraFacingDirs[] = {
                right,
                -right,
                up,
                -up,
                -forward,
                forward
            };
            glm::vec3 lookAtDir = tangentFrame * LightProbeCameras::cameraFacingDirections[face];
            PerspectiveCamera radianceCubeCam(
                glm::vec3(radianceCube.position),
                glm::vec3(radianceCube.position) + lookAtDir,
                tangentFrame * LightProbeCameras::worldUps[face],
                90.f,
                0.01f,
                32.f,
                1.0f
            );

            customRenderScene(radianceCube, radianceCubeCam);
        }
    }

    void ManyViewGI::customRenderScene(const Hemicube& hemicube, const PerspectiveCamera& camera) {
        Shader* shader = ShaderManager::createShader({ 
            ShaderSource::Type::kVsPs,
            "ManyViewGIShader",
            SHADER_SOURCE_PATH "manyview_gi_final_gather_v.glsl", 
            SHADER_SOURCE_PATH "manyview_gi_final_gather_p.glsl",
        });

        // suppose this only need to be set once
        for (auto light : m_scene->lights) {
            light->setShaderParameters(shader);
        }

        m_gfxc->setShader(shader);
        shader->setUniform("view", camera.view());
        shader->setUniform("projection", camera.projection());
        shader->setUniform("receivingNormal", vec4ToVec3(hemicube.normal));
        shader->setUniform("microBufferRes", finalGatherRes);
        m_renderer->submitSceneMultiDrawIndirect(*m_scene);
    }

    void ManyViewGI::resampleRadiance(TextureCubeRenderable* radianceCubemap, glm::uvec2 index) {
        Viewport viewport = { index.x * resampledMicroBufferRes, index.y * resampledMicroBufferRes, resampledMicroBufferRes, resampledMicroBufferRes };
        Shader* octMappingShader = ShaderManager::createShader({ ShaderType::kVsPs, "OctMappingShader", SHADER_SOURCE_PATH "oct_mapping_v.glsl", SHADER_SOURCE_PATH "oct_mapping_p.glsl" });
        m_gfxc->setShader(octMappingShader);
        m_renderer->submitScreenQuadPass(
            radianceAtlasRenderTarget,
            viewport,
            octMappingShader,
            [this, radianceCubemap](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("srcCubemap", radianceCubemap);
                shader->setUniform("outputResolution", glm::vec2(resampledMicroBufferRes));
            }
        );
    }

    // todo: performance of this pass probably needs some improvements!
    // calculate irradiance for a single hemicube
    void ManyViewGI::convolveIrradiance(const glm::ivec2& hemicubeCoord, TextureCubeRenderable* radianceCubemap) {
        auto shader = ShaderManager::createShader({ 
            ShaderSource::Type::kCs,
            "ManyViewGIConvolveIrradianceShader",
            nullptr,
            nullptr,
            nullptr,
            SHADER_SOURCE_PATH "manyview_gi_convolve_irradiance_c.glsl",
        });
        m_gfxc->setShader(shader);
        shader->setUniform("hemicubeCoord", hemicubeCoord);
        shader->setUniform("microBufferRes", finalGatherRes);
        shader->setUniform("resampledMicroBufferRes", resampledMicroBufferRes);
        shader->setTexture("radianceCubemap", radianceCubemap);
        glBindImageTexture(0, irradianceAtlas->getGpuResource(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        glDispatchCompute(1, 1, 1);
    }

    void ManyViewGI::progressiveBuildRadianceAtlas(SceneRenderable& scene) {
        bool bBuildingComplete = false;
        for (i32 i = 0; i < perFrameWorkload; ++i) {
            i32 hemicubeIndex = (perFrameWorkload * renderedFrames + i);
            if (hemicubeIndex < maxNumHemicubes) {
                if (hemicubes[hemicubeIndex].position.w > 0.f) {
                    i32 x = hemicubeIndex % irradianceAtlasRes.x;
                    i32 y = hemicubeIndex / irradianceAtlasRes.x;
                    glm::vec2 jitter = halton23(hemicubeIndex);
                    gatherRadiance(hemicubes[hemicubeIndex], hemicubeIndex, true, scene, radianceCubemaps[hemicubeIndex]);
                    resampleRadiance(radianceCubemaps[hemicubeIndex], glm::uvec2(x, y));
                    convolveIrradiance(glm::ivec2(x, y), radianceCubemaps[hemicubeIndex]);
                }
            }
            else {
                bBuildingComplete = true;
                break;
            }
        }
        if (bBuildingComplete) {
            renderedFrames = 0;
            bBuilding = false;
            m_renderer->gaussianBlur(radianceAtlas, 2, 1.f);
            m_renderer->gaussianBlur(radianceAtlas, 3, 2.f);
            m_renderer->gaussianBlur(radianceAtlas, 4, 3.f);
            m_renderer->gaussianBlur(radianceAtlas, 5, 4.f);
            m_renderer->gaussianBlur(radianceAtlas, 6, 5.f);
        }
        else {
            renderedFrames++;
        }
    }

    void ManyViewGI::convolveIrradianceBatched() {
        Shader* convolveIrradianceShader = ShaderManager::createShader({ 
            ShaderSource::Type::kCs,
            "RasterGIConvolveIrradianceShader",
            nullptr,
            nullptr,
            nullptr,
            SHADER_SOURCE_PATH "raster_gi_convolve_irradiance_c.glsl",
        });
        m_gfxc->setShader(convolveIrradianceShader);
        convolveIrradianceShader->setUniform("microBufferRes", resampledMicroBufferRes);
        convolveIrradianceShader->setTexture("radianceAtlas", radianceAtlas);
        glBindImageTexture(0, irradianceAtlas->getGpuResource(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        glDispatchCompute(irradianceAtlasRes.x, irradianceAtlasRes.y, 1);
    }

    // visualize all hemicubes
    void ManyViewGI::visualizeHemicubes(RenderTarget* dstRenderTarget) {
        static ShaderStorageBuffer<DynamicSsboData<u64>> cubemapHandleBuffer(maxNumHemicubes);
        static ShaderStorageBuffer<DynamicSsboData<glm::mat4>> surfelInstanceBuffer(maxNumHemicubes);
        struct Line {
            glm::mat4 v0;
            glm::mat4 v1;
            glm::vec4 albedo;
        };
        static ShaderStorageBuffer<DynamicSsboData<Line>> lineBuffer(3 * maxNumHemicubes);

        for (i32 i = 0; i < maxNumHemicubes; ++i) {
            surfelInstanceBuffer[i] = calcHemicubeTransform(hemicubes[i], glm::vec3(0.05));
            if (glIsTextureHandleResidentARB(radianceCubemaps[i]->glHandle) == GL_FALSE) {
                glMakeTextureHandleResidentARB(radianceCubemaps[i]->glHandle);
            }
            cubemapHandleBuffer[i] = radianceCubemaps[i]->glHandle;
            glm::mat4 tangentFrame = calcHemicubeTangentFrame(hemicubes[i]);
            glm::vec4 colors[3] = {
                glm::vec4(1.f, 0.f, 0.f, 1.f),
                glm::vec4(0.f, 1.f, 0.f, 1.f),
                glm::vec4(0.f, 0.f, 1.f, 1.f)
            };
            for (i32 j = 0; j < 3; ++j) {
                lineBuffer[i * 3 + j].v0 = glm::translate(glm::mat4(1.f), vec4ToVec3(hemicubes[i].position));
                lineBuffer[i * 3 + j].v1 = glm::translate(glm::mat4(1.f), vec4ToVec3(hemicubes[i].position + tangentFrame[j] * 0.1f));
                lineBuffer[i * 3 + j].albedo = colors[j];
            }
        }

        surfelInstanceBuffer.upload();
        surfelInstanceBuffer.bind(63);
        cubemapHandleBuffer.upload();
        cubemapHandleBuffer.bind(64);
        lineBuffer.upload();
        lineBuffer.bind(65);

        m_gfxc->setRenderTarget(dstRenderTarget);
        m_gfxc->setDepthControl(DepthControl::kEnable);
        m_gfxc->setViewport({ 0, 0, dstRenderTarget->width, dstRenderTarget->height });
        auto shader = ShaderManager::createShader({
            ShaderSource::Type::kVsPs,
            "VisualizeHemicubesShader",
            SHADER_SOURCE_PATH "debug_show_hemicubes_v.glsl",
            SHADER_SOURCE_PATH "debug_show_hemicubes_p.glsl"
            });
        m_gfxc->setShader(shader);
        shader->setUniform("microBufferRes", finalGatherRes);
        auto cube = AssetManager::getAsset<Mesh>("UnitCubeMesh");
        m_gfxc->setVertexArray(cube->getSubmesh(0)->getVertexArray());
        glDisable(GL_CULL_FACE);
        // draw hemicubes
        glDrawArraysInstanced(GL_TRIANGLES, 0, cube->getSubmesh(0)->numVertices(), maxNumHemicubes);
        glEnable(GL_CULL_FACE);

        auto lineShader = ShaderManager::createShader({
            ShaderSource::Type::kVsPs,
            "DebugDrawLineShader",
            SHADER_SOURCE_PATH "debug_draw_line_v.glsl",
            SHADER_SOURCE_PATH "debug_draw_line_p.glsl"
            });
        m_gfxc->setShader(lineShader);
        // draw tangent frames
        glDrawArraysInstanced(GL_LINES, 0, 2, maxNumHemicubes * 3);
    }

    void ManyViewGI::visualizeIrradiance(RenderTarget* dstRenderTarget) {
        auto disk = AssetManager::getAsset<Mesh>("Disk");
        static ShaderStorageBuffer<DynamicSsboData<glm::vec2>> irradianceCoordBuffer(maxNumHemicubes);
        static ShaderStorageBuffer<DynamicSsboData<glm::mat4>> surfelInstanceBuffer(maxNumHemicubes);
        u32 numActiveHemicubes = 0;
        for (i32 i = 0; i < maxNumHemicubes; ++i) {
            if (hemicubes[i].position.w > 0.f) {
                f32 x = (f32)(i % irradianceAtlas->width) / irradianceAtlas->width;
                f32 y = (f32)(i / irradianceAtlas->width) / irradianceAtlas->height;
                irradianceCoordBuffer[numActiveHemicubes] = glm::vec2(x, y);
                glm::mat4 transform = glm::translate(glm::mat4(1.f), vec4ToVec3(hemicubes[i].position));
                transform = transform * calcHemicubeTangentFrame(hemicubes[i]);
                transform = glm::scale(transform, glm::vec3(0.1));
                surfelInstanceBuffer[numActiveHemicubes] = transform;
                numActiveHemicubes++;
            }
        }
        surfelInstanceBuffer.upload();
        surfelInstanceBuffer.bind(67);

        irradianceCoordBuffer.upload();
        irradianceCoordBuffer.bind(66);

        auto shader = ShaderManager::createShader({ 
            ShaderSource::Type::kVsPs,
            "VisualizeIrradianceShader",
            SHADER_SOURCE_PATH "visualize_irradiance_v.glsl",
            SHADER_SOURCE_PATH "visualize_irradiance_p.glsl"
            });
        m_gfxc->setRenderTarget(dstRenderTarget);
        m_gfxc->setViewport({ 0, 0, dstRenderTarget->width, dstRenderTarget->height });
        shader->setTexture("irradianceBuffer", irradianceAtlas);
        m_gfxc->setShader(shader);
        auto va = disk->getSubmesh(0)->getVertexArray();
        m_gfxc->setVertexArray(va);
        if (va->hasIndexBuffer()) {
            glDrawElementsInstanced(GL_TRIANGLES, disk->getSubmesh(0)->numIndices(), GL_UNSIGNED_INT, 0, numActiveHemicubes);
        }
        else {
            glDrawArraysInstanced(GL_TRIANGLES, 0, disk->getSubmesh(0)->numVertices(), numActiveHemicubes);
        }
    }

    PointBasedManyViewGI::PointBasedManyViewGI(Renderer* renderer, GfxContext* gfxc)
        : ManyViewGI(renderer, gfxc) {

    }

    void PointBasedManyViewGI::customInitialize() { 
        if (!bInitialized) {
            if (!visualizations.rasterizedSurfels) {
                ITextureRenderable::Spec spec = {};
                spec.type = TEX_2D;
                spec.width = 16;
                spec.height = 16;
                spec.pixelFormat = PF_RGB16F;
                visualizations.rasterizedSurfels = new Texture2DRenderable("RasterizedSurfelScene", spec);
            }
            bInitialized = true;
        }
    }

    void PointBasedManyViewGI::customSetup(const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) {
        generateWorldSpaceSurfels();
        cacheSurfelDirectLighting();
    }

    void PointBasedManyViewGI::customRender(RenderTarget* sceneRenderTarget, RenderTarget* visRenderTarget) {
        if (m_scene) {
            if (bVisualizeSurfels) {
                visualizeSurfels(visRenderTarget);
                rasterizeSurfelScene(visualizations.rasterizedSurfels, m_scene->camera);
            }
        }
    }

    void PointBasedManyViewGI::customUI() {
        ImGui::Checkbox("Visualize Surfels", &bVisualizeSurfels);
    }

    // todo: implement this on gpu using geometry shader
    void PointBasedManyViewGI::generateWorldSpaceSurfels() {
        surfels.clear();
        surfelBuffer.data.array.clear();
        // calculate total surface area by summing area of all triangles in the scene
        for (i32 i = 0; i < m_scene->meshInstances.size(); ++i) {
            auto meshInst = m_scene->meshInstances[i];
            auto mesh = meshInst->parent;
            glm::mat4 transform = (*m_scene->transformSsbo)[i];
            glm::mat4 normalTransform = glm::inverse(glm::transpose(transform));
            for (i32 sm = 0; sm < mesh->numSubmeshes(); ++sm) {
                auto submesh = dynamic_cast<Mesh::Submesh<Triangles>*>(mesh->getSubmesh(sm));
                if (submesh) {
                    glm::vec3 albedo(0.f);
                    auto matl = dynamic_cast<PBRMaterial*>(meshInst->getMaterial(sm));
                    // todo: figure out how to handle textured material
                    if (matl) {
                        albedo = matl->parameter.kAlbedo;
                    }
                    const auto& verts = submesh->getVertices();
                    const auto& indices = submesh->getIndices();
                    i32 numTriangles = submesh->numIndices() / 3;
                    for (i32 t = 0; t < numTriangles; ++t) {
                        const auto& v0 = verts[indices[(u64)t * 3 + 0]];
                        const auto& v1 = verts[indices[(u64)t * 3 + 1]];
                        const auto& v2 = verts[indices[(u64)t * 3 + 2]];

                        // transform vertices
                        glm::vec3 p0 = transform * glm::vec4(v0.pos, 1.f);
                        glm::vec3 p1 = transform * glm::vec4(v1.pos, 1.f);
                        glm::vec3 p2 = transform * glm::vec4(v2.pos, 1.f);

                        // transform normal
                        glm::vec3 n0 = glm::normalize(normalTransform * glm::vec4(v0.normal, 0.f));
                        glm::vec3 n1 = glm::normalize(normalTransform * glm::vec4(v1.normal, 0.f));
                        glm::vec3 n2 = glm::normalize(normalTransform * glm::vec4(v2.normal, 0.f));

                        // calculate triangle area
                        f32 area = .5f * glm::length(glm::cross(p1 - p0, p2 - p0));
                        // todo: this doesn't work for densely tessellated meshes such as the stanford bunny and dragon meshes
                        i32 numSurfels = (area * 2.f) / (3.1415926f * kSurfelRadius * kSurfelRadius);
                        for (i32 s = 0; s < numSurfels; ++s) {
                            glm::vec2 st = halton23(s);
                            // rejection sampling
                            i32 numRejected = 0;
                            while (st.x + st.y > 1.f) {
                                st = halton23(s + numRejected);
                                numRejected++;
                            }
                            f32 w = 1.f - st.x - st.y;
                            surfels.emplace_back(Surfel{
                                st.x * p0 + st.y * p1 + w * p2,
                                glm::normalize(st.x * n0 + st.y * n1 + w * n2),
                                albedo,
                                kSurfelRadius,
                            });
                            surfelBuffer.addElement(GpuSurfel{
                                glm::vec4(st.x * p0 + st.y * p1 + w * p2, 1.f),
                                glm::vec4(glm::normalize(st.x * n0 + st.y * n1 + w * n2), 0.f),
                                glm::vec4(albedo, 1.f),
                                glm::vec4(0.f, 0.f, 0.f, 1.f)
                            });
                        }
                    }
                }
            }
        }
        surfelBuffer.upload();
        updateSurfelInstanceData();
    }

    void PointBasedManyViewGI::cacheSurfelDirectLighting() {
        if (m_scene) {
            auto shader = ShaderManager::createShader({
                ShaderSource::Type::kCs,
                "RenderSurfelDirectLightingShader",
                nullptr,
                nullptr,
                nullptr,
                SHADER_SOURCE_PATH "surfel_direct_lighting_c.glsl"
            });

            surfelBuffer.bind(69);

            // suppose this only need to be set once
            for (auto light : m_scene->lights) {
                light->setShaderParameters(shader);
            }

            m_gfxc->setShader(shader);
            // each compute thread process one surfel
            glDispatchCompute(surfels.size(), 1, 1);
        }
    }

    void PointBasedManyViewGI::rasterizeSurfelScene(Texture2DRenderable* outSceneColor, const RenderableCamera& camera) {
        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(outSceneColor->width, outSceneColor->height));
        renderTarget->setColorBuffer(outSceneColor, 0);
        renderTarget->setDrawBuffers({ 0 });
        renderTarget->clearDrawBuffer(0, glm::vec4(0.2f, .2f, .2f, 1.f));

        auto shader = ShaderManager::createShader({
            ShaderSource::Type::kVsPs,
            "RasterizeSurfelShader",
            SHADER_SOURCE_PATH "rasterize_surfels_v.glsl",
            SHADER_SOURCE_PATH "rasterize_surfels_p.glsl"
        });

        assert(surfels.size() == surfelBuffer.getNumElements());

        surfelBuffer.bind(69);

        m_gfxc->setShader(shader);
        m_gfxc->setRenderTarget(renderTarget.get());
        m_gfxc->setViewport({ 0, 0, renderTarget->width, renderTarget->height });
        shader->setUniform("outputSize", glm::ivec2(renderTarget->width, renderTarget->height));
        shader->setUniform("cameraFov", camera.fov);
        shader->setUniform("cameraN", camera.n);
        shader->setUniform("view", camera.view);
        shader->setUniform("projection", camera.projection);
        shader->setUniform("receivingNormal", m_scene->camera.view[1]);
        glDrawArraysInstanced(GL_POINTS, 0, 1, surfels.size());
    }

    void PointBasedManyViewGI::customRenderScene(const Hemicube& hemicube, const PerspectiveCamera& camera) {
        auto shader = ShaderManager::createShader({
            ShaderSource::Type::kVsPs,
            "RasterizeSurfelShader",
            SHADER_SOURCE_PATH "rasterize_surfels_v.glsl",
            SHADER_SOURCE_PATH "rasterize_surfels_p.glsl"
        });
        assert(surfels.size() == surfelBuffer.getNumElements());

        surfelBuffer.bind(69);

        shader->setUniform("outputSize", glm::ivec2(finalGatherRes, finalGatherRes));
        shader->setUniform("cameraFov", camera.fov);
        shader->setUniform("cameraN", camera.n);
        shader->setUniform("surfelRadius", kSurfelRadius);
        shader->setUniform("view", camera.view());
        shader->setUniform("projection", camera.projection());
        shader->setUniform("hemicubeNormal", vec4ToVec3(hemicube.normal));
        m_gfxc->setShader(shader);
        glDrawArraysInstanced(GL_POINTS, 0, 1, surfels.size());
    }

    void PointBasedManyViewGI::updateSurfelInstanceData() {
        surfelInstanceBuffer.data.array.resize(surfels.size());
        for (i32 i = 0; i < surfels.size(); ++i) {
            glm::mat4 transform = glm::translate(glm::mat4(1.f), surfels[i].position);
            transform = transform * calcTangentFrame(surfels[i].normal);
            surfelInstanceBuffer[i].transform = glm::scale(transform, glm::vec3(surfels[i].radius));
            surfelInstanceBuffer[i].albedo = glm::vec4(surfels[i].albedo, 1.f);
            surfelInstanceBuffer[i].radiance = glm::vec4(0.f, 0.f, 0.f, 1.f);
            surfelInstanceBuffer[i].debugColor = glm::vec4(1.f, 0.f, 0.f, 1.f);
        }
        surfelInstanceBuffer.upload();
    }

    void PointBasedManyViewGI::visualizeSurfels(RenderTarget* visRenderTarget) {
        surfelInstanceBuffer.bind(68);
        auto disk = AssetManager::getAsset<Mesh>("Disk");
        auto shader = ShaderManager::createShader({ 
            ShaderSource::Type::kVsPs,
            "VisualizeSurfelShader",
            SHADER_SOURCE_PATH "visualize_surfel_v.glsl",
            SHADER_SOURCE_PATH "visualize_surfel_p.glsl"
            });
        m_gfxc->setRenderTarget(visRenderTarget);
        m_gfxc->setViewport({ 0, 0, visRenderTarget->width, visRenderTarget->height });
        m_gfxc->setShader(shader);
        shader->setUniform("visMode", (u32)(m_visMode));
        auto va = disk->getSubmesh(0)->getVertexArray();
        m_gfxc->setVertexArray(va);
        if (va->hasIndexBuffer()) {
            glDrawElementsInstanced(GL_TRIANGLES, disk->getSubmesh(0)->numIndices(), GL_UNSIGNED_INT, 0, surfelInstanceBuffer.getNumElements());
        }
        else {
            glDrawArraysInstanced(GL_TRIANGLES, 0, disk->getSubmesh(0)->numVertices(), surfelInstanceBuffer.getNumElements());
        }
    }

    MicroRenderingGI::MicroRenderingGI(Renderer* renderer, GfxContext* gfxc)
        : PointBasedManyViewGI(renderer, gfxc) {

    }

    void MicroRenderingGI::customInitialize() {
        PointBasedManyViewGI::customInitialize();

        if (!bInitialized) {
            bInitialized = true;
        }
    }

    void MicroRenderingGI::customSetup(const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) {
        PointBasedManyViewGI::customSetup(scene, depthBuffer, normalBuffer);
        m_surfelBSH.build(surfels);
    }

    void MicroRenderingGI::generateWorldSpaceSurfels() {
        m_surfelSampler.sampleFixedSizeSurfels(surfels, *m_scene);
        for (const auto& surfel : surfels) {
            surfelBuffer.addElement(
                GpuSurfel{
                    glm::vec4(surfel.position, surfel.radius),
                    glm::vec4(surfel.normal, 0.f),
                    glm::vec4(surfel.albedo, 1.f),
                    glm::vec4(0.f, 0.f, 0.f, 1.f)
                }
            );
        }
        surfelBuffer.upload();
        m_surfelBSH.build(surfels);
    }

    void MicroRenderingGI::customRender(RenderTarget* sceneRenderTarget, RenderTarget* visRenderTarget) {
        PointBasedManyViewGI::customRender(sceneRenderTarget, visRenderTarget);
        if (bVisualizeSurfelTree) {
            m_surfelBSH.visualize(m_gfxc, visRenderTarget);
        }
        m_surfelSampler.visualize(sceneRenderTarget, m_renderer);
    }

    void MicroRenderingGI::microRendering(const PerspectiveCamera& camera, SurfelBSH& surfelBSH) {
        static Texture2DRenderable* microBuffer = nullptr;
        if (!microBuffer) {
            ITextureRenderable::Spec spec = { };
            spec.type = TEX_2D;
            spec.width = 16;
            spec.height = 16;
            spec.pixelFormat = PF_RGBA16F;
            microBuffer = new Texture2DRenderable("DebugMicroBuffer", spec);
        }
        auto shader = ShaderManager::createShader({ 
            ShaderSource::Type::kCs,
            "MicroRenderingShader",
            nullptr,
            nullptr,
            nullptr,
            SHADER_SOURCE_PATH "micro_rendering_c.glsl",
        });
        surfelBSH.gpuNodes.bind(64);
        shader->setUniform("view", camera.view());
        shader->setUniform("projection", camera.projection());
    }

    void MicroRenderingGI::customUI() {
        PointBasedManyViewGI::customUI();
        ImGui::Checkbox("Visualize Surfel BSH:", &bVisualizeSurfelTree);
        if (bVisualizeSurfelTree) {
            ImGui::Checkbox("Visualize Bounding Spheres:", &m_surfelBSH.bVisualizeBoundingSpheres);
            ImGui::Checkbox("Visualize Nodes:", &m_surfelBSH.bVisualizeNodes);
            ImGui::Text("Visualize Level");
            ImGui::SliderInt("##Visualize Level", &m_surfelBSH.activeVisLevel, 0, m_surfelBSH.getNumLevels() - 1);
        }
    }
}