#include "stb_image_write.h"

#include "RayMarchingHeightField.h"
#include "World.h"
#include "SceneCameraEntity.h"
#include "SceneCameraComponent.h"
#include "CameraControllerComponent.h"
#include "LightEntities.h"
#include "Noise.h"
#include "AssetManager.h"
#include "Image.h"
#include "GHTexture.h"
#include "GfxModule.h"
#include "RenderPass.h"
#include "RenderingUtils.h"
#include "Texture.h"
#include "Shader.h"


// todo: infinite tiling
// todo: handle transform of the height field
// todo: shading
namespace Cyan
{
    class HeightField
    {
    public:
        HeightField()
            : m_worldSpacePosition(glm::vec3(0.f))
            , m_scale(glm::vec3(1.f))
            , m_aabb()
            , m_heightValues(size * size)
        {
            /**
             * Default dimension of the height field in its local space
             */
            m_aabb.bound(glm::vec3(-1.f, 0.f, -1.f));
            m_aabb.bound(glm::vec3( 1.f, 0.f,  1.f));
        }

        ~HeightField()
        {
        }

        void build()
        {
        }

        static constexpr i32 size = 256;

        glm::vec3 m_worldSpacePosition = glm::vec3(0.f);
        glm::vec3 m_scale = glm::vec3(1.f);
        AxisAlignedBoundingBox3D m_aabb;
        std::vector<f32> m_heightValues;
    };

#define APP_SHADER_PATH  "C:/dev/cyanRenderEngine/EngineApp/Apps/RayMarchingHeightField/Shader/"

    RayMarchingHeightFieldApp::RayMarchingHeightFieldApp(i32 windowWidth, i32 windowHeight)
        : App("RayMarchingHeightField", windowWidth, windowHeight)
    {

    }

    RayMarchingHeightFieldApp::~RayMarchingHeightFieldApp()
    {

    }

    void RayMarchingHeightFieldApp::update(World* world)
    {
        // add a pass to perform ray marching into the height field
        Engine::get()->enqueueFrameGfxTask("RayMarchingHeightField", [this](Frame& frame) {
            static Transform s_heightFieldTransform;

            auto views = *(frame.views);
            auto currentActiveView = views[0];

            const auto& viewState = currentActiveView->getState();
            const auto& cameraInfo = currentActiveView->getCameraInfo();

            glm::vec3 cameraPosition = viewState.cameraPosition;
            auto desc = m_rayMarchingOutTexture->getDesc();

            bool found;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "RayMarchingHeightField", APP_SHADER_PATH "raymarching_heightfield_p.glsl");
            auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

            RenderingUtils::renderScreenPass(
                glm::uvec2(desc.width, desc.height),
                [this](RenderPass& rp) {
                    rp.setRenderTarget(m_rayMarchingOutTexture.get(), 0);
                },
                gfxp.get(),
                [this, cameraInfo, viewState, desc](GfxPipeline* p) {
                    p->setUniform("cameraPosition", viewState.cameraPosition);
                    p->setUniform("cameraRight", viewState.cameraRight);
                    p->setUniform("cameraForward", viewState.cameraForward);
                    p->setUniform("cameraUp", viewState.cameraUp);
                    p->setUniform("n", cameraInfo.m_perspective.n);
                    p->setUniform("f", cameraInfo.m_perspective.f);
                    p->setUniform("fov", cameraInfo.m_perspective.fov);
                    p->setUniform("aspectRatio", (f32)desc.width / desc.height);
                    p->setTexture("heightFieldTex", m_heightFieldTexture.get());
                }
            );
        });
    }

    // todo:
    static void buildPerlinNoiseHeightField()
    {
        const i32 heightFieldSize = 256;
    }

    void RayMarchingHeightFieldApp::customInitialize(World* world)
    {
        const char* sceneFilePath = "C:/dev/cyanRenderEngine/EngineApp/Apps/RayMarchingHeightField/Resources/Scene.glb";
        world->import(sceneFilePath);

        // add a camera
        Transform cameraTransform = { };
        cameraTransform.translation = glm::vec3(0.f, 4.f, 5.f); 
        SceneCameraEntity* ce = world->createEntity<SceneCameraEntity>("TestCamera_0", cameraTransform).get();
        auto cc = ce->getSceneCameraComponent();
        cc->setResolution(glm::uvec2(1920, 1080));
        cc->setRenderMode(SceneCamera::RenderMode::kResolvedSceneColor);
        auto cameraControllerComponent = std::make_shared<CameraControllerComponent>("CameraControllerComponent", cc);
        ce->addComponent(cameraControllerComponent);

        // add a directional light
        DirectionalLightEntity* dle = world->createEntity<DirectionalLightEntity>("SunLight", Transform()).get();

        // generate a 2D height map using Perlin noise 
        // todo: what parameters can be used to control the shape of the perlin noise
        const i32 heightMapSize = 256;
        std::vector<f32> heightValues(heightMapSize * heightMapSize);
        for (i32 y = 0; y < heightMapSize; ++y)
        {
            for (i32 x = 0; x < heightMapSize; ++x)
            {
                glm::dvec3 p(x, y, 0);
                p = (p + .5) / (f64)heightMapSize;
                p.x = p.x * 2. - 1.;
                p.y = p.y * 2. - 1.;

                i32 outX = x;
                i32 outY = heightMapSize - y - 1;
                i32 outIndex = heightMapSize * outY + x;

                heightValues[outIndex] = PerlineNoise(p);
            }
        }

        Engine::get()->enqueueFrameGfxTask("CreateTextures", [this, heightValues](Frame& frame) mutable {
            {
                auto desc = GHTexture2D::Desc::create(heightMapSize, heightMapSize, 1, PF_R32F, heightValues.data());
                GHSampler2D sampler2D;
                sampler2D.setAddressingModeX(SAMPLER2D_AM_WRAP);
                sampler2D.setAddressingModeY(SAMPLER2D_AM_WRAP);
                m_heightFieldTexture = std::move(GHTexture2D::create(desc, sampler2D));
            }
            {
                glm::uvec2 windowSize = GfxModule::get()->getWindowSize();
                auto desc = GHTexture2D::Desc::create(windowSize.x, windowSize.y, 1, PF_RGB16F);
                GHSampler2D sampler2D;
                sampler2D.setAddressingModeX(SAMPLER2D_AM_CLAMP);
                sampler2D.setAddressingModeY(SAMPLER2D_AM_CLAMP);
                m_rayMarchingOutTexture = std::move(GHTexture2D::create(desc, sampler2D));
            }
        });

        Engine::get()->enqueueFrameGfxTask("SetPostRenderSceneViewFunc", [this](Frame& frame) {
            GfxModule::get()->setPostRenderSceneViews([this](std::vector<SceneView*>* views) {
                // GfxModule::s_defaultPostRenderSceneViews(views);

                glm::uvec2 windowSize = GfxModule::get()->getWindowSize();
                RenderingUtils::renderToBackBuffer(m_rayMarchingOutTexture.get());
                RenderingUtils::renderToBackBuffer(m_heightFieldTexture.get(), Viewport { 0, (i32)windowSize.y - 256, 256, 256 });
                });
            });

#if 0
        // save the height map to disk as an image
        if (!stbi_write_hdr("C:/dev/cyanRenderEngine/EngineApp/Apps/RayMarchingHeightField/Resources/Heightmap.hdr", 256, 256, 1, heightValues.data()))
        {
            assert(0);
        }
#endif
    }
}
