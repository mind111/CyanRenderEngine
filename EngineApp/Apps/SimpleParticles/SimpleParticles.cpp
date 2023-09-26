#include <chrono>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include "glew/glew.h"

#include "SimpleParticles.h"
#include "RenderPass.h"
#include "GfxModule.h"
#include "RenderingUtils.h"
#include "World.h"
#include "SceneRender.h"
#include "SceneCameraEntity.h"
#include "LightEntities.h"
#include "SceneCameraComponent.h"
#include "CameraControllerComponent.h"
#include "Color.h"
#include "Transform.h"
#include "AssetManager.h"
#include "StaticMesh.h"

#define APP_SHADER_PATH "C:/dev/cyanRenderEngine/EngineApp/Apps/SimpleParticles/Shader/"

namespace Cyan
{
    // particle settings
    static f32 maxSpeed = 5.f;
    static f32 maxLife = 0.5f;
    static f32 minSize = 0.05f;
    static f32 maxSize = 2.f;
    static f32 size = 1.f;
    static f32 emitConeAngle = 10.f;
    static f32 spawnInterval = .01f;
    static f32 elapsedTimeSeconds = 0.f;
    static f32 deltaTimeInSeconds = 0.f;

    struct Particle
    {
        glm::vec3 position;
        glm::vec3 velocity;
        glm::vec3 color;
        f32 size; 
        f32 life; // in seconds
        f32 rotationInRadians;
    };

    struct ParticleEmitter
    {
        enum EmittType
        {
            kHemiSpherical,
            kOmniDirectional,
            kSingleDirectional
        };

        void emit(std::vector<Particle>& particles, f32 elapsedTimeInSeconds)
        {
            if ((elapsedTimeInSeconds - lastEmitTimeStamp) >= emitTimeInterval)
            {
                for (i32 i = 0; i < 1; ++i)
                {
                    f32 randAngleInRadians = uniformSampleZeroToOne() * glm::pi<f32>() * 2.f;
                    glm::vec3 spawnDirection = uniformSampleCone(glm::vec3(0.f, 1.f, 0.f), emitConeAngle);
                    particles.push_back(Particle{
                        position,
                        spawnDirection * maxSpeed * uniformSampleZeroToOne(),
                        Color::randomColor(),
                        size, // size
                        maxLife, // life
                        randAngleInRadians
                    });
                }
                lastEmitTimeStamp = elapsedTimeInSeconds;
            }
        }

        EmittType type = kHemiSpherical;
        glm::vec3 position = glm::vec3(0.f);
        f32 lastEmitTimeStamp = 0.f;
        f32 emitTimeInterval = 0.01f;
        i32 numParticlesToEmit = 1;
    };

    struct ParticleSystem
    {
        ParticleSystem()
        {
            prev = std::chrono::steady_clock::now();
            curr = std::chrono::steady_clock::now();
            elapsedTime = 0.f;

            Engine::get()->enqueueFrameGfxTask(
                RenderingStage::kPreSceneRendering,
                "CreateParticleSprite",
                [this](Frame& frame) {
                    const glm::uvec2 spriteSize(128, 128);
                    GHSampler2D spriteSampler(SamplerAddressingMode::Clamp, SamplerAddressingMode::Clamp, SamplerFilteringMode::Bilinear, SamplerFilteringMode::Bilinear);
                    auto spriteDesc = GHTexture2D::Desc::create(spriteSize.x, spriteSize.y, 1, PF_RGBA16F);
                    sprite = std::move(GHTexture2D::create(spriteDesc, spriteSampler));
                }
            );
        }

        ParticleSystem(const ParticleSystem& rhs)
        {
            emitter = rhs.emitter;
            particles = rhs.particles;
            prev = rhs.prev;
            curr = rhs.curr;
            elapsedTime = rhs.elapsedTime;
            sprite = rhs.sprite;
            frequency = rhs.frequency;
            persistence = rhs.persistence;
        }

        void update()
        {
            curr = std::chrono::steady_clock::now();
            f32 deltaTimeInSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(curr - prev).count() / 1000.f;
            elapsedTimeSeconds += deltaTimeInSeconds;
            prev = curr;

            emitter.emit(particles, elapsedTimeSeconds);

            for (i32 i = particles.size() - 1; i >= 0; i--)
            {
                auto& particle = particles[i];
                particle.position += particle.velocity * deltaTimeInSeconds;
                particle.life -= deltaTimeInSeconds;

                f32 t = 1.f - particle.life / maxLife;

                if (particle.life < 0.f)
                {
                    particles.erase(particles.begin() + i);
                }
            }
        }

        void render()
        {
#if 0
            Engine::get()->addSceneRenderingTask(
                SceneRenderingStage::kPostLighting,
                "RenderCubeParticles",
                [this](SceneView& view) {
                    auto render = view.getRender();
                    const auto& viewState = view.getState();
                    const auto& cameraInfo = view.getCameraInfo();

                    GHTexture2D* output = render->directLighting();

                    bool found = false;
                    auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "StaticMeshVS", ENGINE_SHADER_PATH "static_mesh_v.glsl");
                    auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "CubeParticlePS", APP_SHADER_PATH "cube_particle_p.glsl");
                    auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

                    auto cubeMesh = RenderingUtils::get()->getUnitCubeMesh();

                    const auto& outputDesc = output->getDesc();
                    RenderPass rp(outputDesc.width, outputDesc.height);
                    RenderTarget rt(output, false);
                    rp.setRenderTarget(rt, 0);
                    DepthTarget dt(render->depth(), false);
                    rp.setDepthTarget(dt);
                    rp.setRenderFunc([gfxp, viewState, cameraInfo, this, cubeMesh](GfxContext* ctx) {
                        gfxp->bind();
                        viewState.setShaderParameters(gfxp.get());
                        for (i32 i = 0; i < particles.size(); i++)
                        {
                            Transform t;
                            t.translation = particles[i].position;
                            t.scale = glm::vec3(0.1f);
                            gfxp->setUniform("localToWorld", t.toMatrix());
                            cubeMesh->draw();
                        }
                        gfxp->unbind();
                    });

                    rp.render(GfxContext::get());
                }
            );
#endif
            renderLambda(*this);
        }

        void bindRenderLambda(const std::function<void(ParticleSystem)>& inRenderLambda)
        {
            renderLambda = inRenderLambda;
        }

        void setPosition(const glm::vec3& position) 
        {
            emitter.position = position;
        }

        ParticleEmitter emitter;
        std::vector<Particle> particles;
        std::chrono::steady_clock::time_point prev;
        std::chrono::steady_clock::time_point curr;
        f32 elapsedTime;
        std::shared_ptr<GHTexture2D> sprite = nullptr;
        f32 frequency = 2.f;
        f32 persistence = .5f;

        std::function<void(ParticleSystem)> renderLambda = [](ParticleSystem system) { };
    };

    static std::array<ParticleSystem, 8> s_fireParticleSystems;
    static std::array<ParticleSystem, 8> s_meshParticleSystems;

    SimpleParticles::SimpleParticles(u32 windowWidth, u32 windowHeight)
        : App("SimpleParticles", windowWidth, windowHeight)
    {
    }

    SimpleParticles::~SimpleParticles()
    {
    }

    void SimpleParticles::update(World* world)
    {
        for (i32 i = 0; i < s_fireParticleSystems.size(); ++i)
        {
            s_fireParticleSystems[i].update();
        }

#if 0
        for (i32 i = 0; i < s_meshParticleSystems.size(); ++i)
        {
            s_meshParticleSystems[i].update();
        }
#endif
    }

    // todos:
    // [x] draw point sprite with a noise texture and random rotation
    // [x] animated noise maybe?
    // [] omni-directional spawner
    // [] solid mesh, particle fire, spark, and try to replicate a vfx tutorial 

    // [] render each particle as ray marched sphere,
    //      [] render one emissive particle this way first

    void SimpleParticles::render()
    {
#if 0
        // draw opaque first
        for (i32 i = 0; i < s_meshParticleSystems.size(); ++i)
        {
            s_meshParticleSystems[i].render();
        }
#endif

        // and then draw transparent
        for (i32 i = 0; i < s_fireParticleSystems.size(); ++i)
        {
            s_fireParticleSystems[i].render();
        }

        Engine::get()->addSceneRenderingTask(
            SceneRenderingStage::kPostLighting,
            "RenderRayMarchedParticles", 
            [](SceneView& view) {

            }
        );
#if 0
        Engine::get()->enqueueUICommand([this](ImGuiContext* imguiCtx) {
            /**
             * This SetCurrentContext() call is necessary here since I'm using DLL, and different DLLs has different
             * ImGuiContext.
             */
            ImGui::SetCurrentContext(imguiCtx);
            ImGui::Begin("SimpleParticles");
            {
                ImGui::Image(sprite->getGHO(), ImVec2(512, 512));
                IMGUI_SLIDER_FLOAT("Frequency:", &frequency, 0.f, 32.f)
                IMGUI_SLIDER_FLOAT("Persistence:", &persistence, 0.f, 1.f)

                IMGUI_SLIDER_FLOAT("Emit Cone Angle:", &emitConeAngle, 0.f, 90.f)
                IMGUI_SLIDER_FLOAT("Size (In Meters):", &size, 0.f, 10.f)
                IMGUI_SLIDER_FLOAT("Max Life (In Seconds):", &maxLife, 0.f, 10.f)
                IMGUI_SLIDER_FLOAT("Spawn Interval (In Seconds):", &spawnInterval, 0.f, 5.f)
            }
            ImGui::End();
        });
#endif
    }

    auto renderFireParticle = [](ParticleSystem particleSystem) mutable {
        static f32 frequency = 2.f;
        static f32 persistence = .5f;

        if (particleSystem.sprite == nullptr)
        {
            return;
        }

        Engine::get()->addSceneRenderingTask(
            SceneRenderingStage::kPostLighting,
            "BuildParticleSprite",
            [particleSystem](SceneView& view) {
                bool found = false;
                auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
                auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "ParticleSpritePS", APP_SHADER_PATH "build_particle_sprite_p.glsl");
                auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);
                const auto& desc = particleSystem.sprite->getDesc();
                RenderingUtils::renderScreenPass(
                    glm::uvec2(desc.width, desc.height),
                    [&particleSystem](RenderPass& pass) {
                        RenderTarget rt(particleSystem.sprite.get());
                        pass.setRenderTarget(rt, 0);
                    },
                    gfxp.get(),
                    [&particleSystem](GfxPipeline* p) {
                        p->setUniform("u_frequency", frequency);
                        p->setUniform("u_persistence", persistence);
                        p->setUniform("u_elapsedTime", particleSystem.elapsedTime);
                    }
                );
            }
        );

        Engine::get()->addSceneRenderingTask(
            SceneRenderingStage::kPostLighting,
            "RenderParticles",
            [particleSystem](SceneView& view) {
                auto render = view.getRender();
                const auto& viewState = view.getState();
                const auto& cameraInfo = view.getCameraInfo();

                static GHTexture2D* output = render->directLighting();
                const auto& outputDesc = output->getDesc();
                static GHTexture2D::Desc blendedDesc = GHTexture2D::Desc::create(outputDesc.width, outputDesc.height, 1, PF_RGBA16F);
                static std::unique_ptr<GHTexture2D> blended(GHTexture2D::create(blendedDesc));

                RenderingUtils::copyTexture(blended.get(), 0, render->directLighting(), 0);

                bool found = false;
                auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "WorldSpacePointVS", ENGINE_SHADER_PATH "worldspace_point_v.glsl");
                auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "ParticlePS", APP_SHADER_PATH "particle_p.glsl");
                auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

                RenderPass rp(blendedDesc.width, blendedDesc.height);
                RenderTarget rt(blended.get());
                rt.bNeedsClear = false;
                rp.setRenderTarget(rt, 0);
                DepthTarget dt(render->depth(), false);
                rp.setDepthTarget(dt);
                rp.setRenderFunc([gfxp, viewState, cameraInfo, &particleSystem](GfxContext* ctx) {
                    static std::vector<PointCloud::Vertex> vertices(1);
                    static std::vector<u32> indices{ 0 };
                    static std::unique_ptr<Geometry> g = std::make_unique<PointCloud>(vertices, indices);
                    static std::unique_ptr<GfxStaticSubMesh> mesh = std::unique_ptr<GfxStaticSubMesh>(GfxStaticSubMesh::create(std::string("drawWorldSpacePointMesh"), g.get()));

                    mesh->updateVertices(0, sizeOfVectorInBytes(vertices), vertices.data());

                    glEnable(GL_BLEND);
                    glBlendFunc(GL_ONE, GL_ONE);

                    gfxp->bind();
                    for (i32 i = 0; i < particleSystem.particles.size(); i++)
                    {
                        // convert point size from world space to screen space unit
                        f32 worldSpaceSize = particleSystem.particles[i].size;
                        f32 projectedSize = worldSpaceSize * cameraInfo.m_perspective.n / glm::abs(particleSystem.particles[i].position.z - viewState.cameraPosition.z);
                        f32 pixelSize = projectedSize * viewState.resolution.x / (cameraInfo.m_perspective.n * glm::tan(cameraInfo.m_perspective.fov * .5f) * cameraInfo.m_perspective.aspectRatio);
                        gfxp->setUniform("u_viewMatrix", viewState.viewMatrix);
                        gfxp->setUniform("u_projectionMatrix", viewState.projectionMatrix);
                        gfxp->setUniform("u_pointSize", pixelSize);
                        f32 alpha = particleSystem.particles[i].life / maxLife;
                        gfxp->setUniform("u_alpha", alpha);
                        gfxp->setTexture("u_sprite", particleSystem.sprite.get());
                        gfxp->setUniform("u_randomRotation", particleSystem.particles[i].rotationInRadians);

                        vertices[0].pos = particleSystem.particles[i].position;
                        vertices[0].color = glm::vec4(particleSystem.particles[i].color, 1.f);
                        mesh->updateVertices(0, sizeOfVectorInBytes(vertices), vertices.data());

                        mesh->draw();
                    }
                    gfxp->unbind();

                    glDisable(GL_BLEND);
                });

                // rp.disableDepthTest();
                rp.enableDepthTest();
                rp.render(GfxContext::get());

                RenderingUtils::copyTexture(render->directLighting(), 0, blended.get(), 0);
            }
        );
    };

    void drawStaticMesh(StaticMesh* sm)
    {
        for (i32 i = 0; i < sm->numSubMeshes(); ++i)
        {
            StaticSubMesh* submesh = (*sm)[i];
            GfxStaticSubMesh* gfxMesh = GfxStaticSubMesh::create(submesh->getName(), submesh->getGeometry());
            gfxMesh->draw();
        }
    }

    auto renderMeshParticles = [](ParticleSystem particleSystem) mutable {
        Engine::get()->addSceneRenderingTask(
            SceneRenderingStage::kPostGBuffer,
            "RenderCubeParticles",
            [particleSystem](SceneView& view) {
                auto render = view.getRender();
                const auto& viewState = view.getState();
                const auto& cameraInfo = view.getCameraInfo();

                GHTexture2D* albedoTex = render->albedo();

                bool found = false;
                auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "StaticMeshVS", ENGINE_SHADER_PATH "static_mesh_v.glsl");
                auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "CubeParticlePS", APP_SHADER_PATH "cube_particle_p.glsl");
                auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

                // auto mesh = RenderingUtils::get()->getUnitCubeMesh();
                // auto mesh = AssetManager::findAsset<StaticMesh>("Sphere");
                auto mesh = AssetManager::findAsset<StaticMesh>("Cube");

                const auto& outputDesc = albedoTex->getDesc();
                RenderPass rp(outputDesc.width, outputDesc.height);
                RenderTarget albedo(render->albedo(), false);
                RenderTarget normal(render->normal(), false);
                RenderTarget metallicRoughness(render->metallicRoughness(), false);
                rp.setRenderTarget(albedo, 0);
                rp.setRenderTarget(normal, 1);
                rp.setRenderTarget(metallicRoughness, 2);
                DepthTarget dt(render->depth(), false);
                rp.setDepthTarget(dt);
                rp.setRenderFunc([gfxp, viewState, cameraInfo, mesh, &particleSystem](GfxContext* ctx) {
                    gfxp->bind();
                    viewState.setShaderParameters(gfxp.get());
                    for (i32 i = 0; i < particleSystem.particles.size(); i++)
                    {
                        Transform t;
                        t.translation = particleSystem.particles[i].position;
                        t.scale = glm::vec3(0.1f);
                        gfxp->setUniform("localToWorld", t.toMatrix());
                        auto instance = mesh->createInstance(t.toMatrix());
                        for (i32 i = 0; i < mesh->numSubMeshes(); ++i)
                        {
                            StaticSubMesh* submesh = (*mesh)[i];
                            GfxStaticSubMesh* gfxMesh = GfxStaticSubMesh::create(submesh->getName(), submesh->getGeometry());
                            gfxMesh->draw();
                        }
                    }
                    gfxp->unbind();
                });
                rp.enableDepthTest();
                rp.render(GfxContext::get());
            }
        );
    };

    void SimpleParticles::customInitialize(World* world)
    {
        const char* sceneFilePath = "C:/dev/cyanRenderEngine/EngineApp/Apps/SimpleParticles/Resources/Scene.glb";
        world->import(sceneFilePath);

        for (i32 i = 0; i < s_fireParticleSystems.size(); ++i)
        {
            s_fireParticleSystems[i].setPosition(glm::vec3(i - 4, 0.f, 0.f));
            s_fireParticleSystems[i].bindRenderLambda(renderFireParticle);
        }

        for (i32 i = 0; i < s_meshParticleSystems.size(); ++i)
        {
            s_meshParticleSystems[i].setPosition(glm::vec3(i - 4, 0.f, 2.f));
            s_meshParticleSystems[i].bindRenderLambda(renderMeshParticles);
        }
    }
}