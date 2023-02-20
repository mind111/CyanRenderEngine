#include <algorithm>

#include "glm.hpp"
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "CyanApp.h"
#include "LightComponents.h"
#include "LightEntities.h"

namespace Cyan
{
    struct RaytracingScene
    {
        struct Camera
        {
            glm::vec3 eye;
            glm::vec3 lookAt;
            glm::vec3 forward;
            glm::vec3 right;
            glm::vec3 up;
            f32 fov;
            f32 aspect;
        };

        struct RectLight
        {

        };

        struct DirectionalLight
        {
            glm::vec3 color;
            f32 intensity;
            glm::vec3 direction;
        };

        struct Primitive
        {
            std::vector<glm::vec3> positions;
            std::vector<glm::vec3> normals;
            std::vector<u32> indices;
        };

        struct Material
        {
            glm::vec3 albedo = glm::vec3(.8f);
            f32 roughness = .8f;
            f32 metallic = 0.f;
        };

        struct Instance
        {
            i32 primitiveID = -1;
            i32 materialID = -1;
            glm::mat4 localToWorld;
        };

        RaytracingScene(const Scene& inScene)
        {
            // building view data
            if (auto inCamera = dynamic_cast<PerspectiveCamera*>(inScene.m_mainCamera->getCamera()))
            {
                camera.eye = inCamera->position;
                camera.lookAt = inCamera->lookAt;
                camera.aspect = inCamera->aspectRatio;
                camera.fov = inCamera->fov;
                const glm::vec3 worldUp = glm::vec3(1.f);
                camera.forward = glm::normalize(camera.lookAt - camera.eye);
                camera.right = glm::cross(camera.forward, worldUp);
                camera.up = glm::cross(camera.right, camera.forward);
            }

            // maps a mesh to index in the primitive array
            std::unordered_map<std::string, u32> meshMap;
            // maps a material to index in the material array
            std::unordered_map<std::string, u32> materialMap;

            for (auto e : inScene.m_entities)
            {
                // building instance data
                if (auto staticMeshEntity = dynamic_cast<StaticMeshEntity*>(e))
                {
                    auto meshInstance = staticMeshEntity->getMeshInstance();
                    auto mesh = meshInstance->mesh;
                    auto meshEntry = meshMap.find(mesh->name);

                    if (meshEntry == meshMap.end())
                    {
                        i32 basePrimitiveID = primitives.size();

                        for (i32 i = 0; i < mesh->numSubmeshes(); ++i)
                        {
                            Instance instance = { };

                            auto sm = mesh->getSubmesh(i);
                            if (auto triangles = dynamic_cast<Triangles*>(sm->geometry))
                            {
                                primitives.emplace_back();

                                Primitive& primitive = primitives.back();
                                primitive.positions.resize(triangles->numVertices() * 3);
                                primitive.normals.resize(triangles->numVertices() * 3);
                                primitive.indices = triangles->indices;

                                // fill in geometry data
                                for (i32 v = 0; v < triangles->numVertices(); ++v)
                                {
                                    const Triangles::Vertex& vertex = triangles->vertices[v];
                                    primitive.positions[v] = vertex.pos;
                                    primitive.normals[v] = vertex.normal;
                                }

                                // get material data
                                Cyan::Material* material = meshInstance->getMaterial(i);
                                auto materialEntry = materialMap.find(material->name);

                                if (materialEntry != materialMap.end())
                                {
                                    instance.materialID = materialEntry->second;
                                }
                                else
                                {
                                    instance.materialID = materials.size();

                                    materials.emplace_back();
                                    Material& newMaterial = materials.back();
                                    newMaterial.albedo = material->albedo;
                                    newMaterial.roughness = material->roughness;
                                    newMaterial.metallic = material->metallic;
                                }

                                instance.primitiveID = basePrimitiveID + i;
                            }

                            instances.push_back(instance);
                        }
                    }
                }
                else if (auto directionalLightEntity = dynamic_cast<DirectionalLightEntity*>(e))
                {
                    Cyan::DirectionalLightComponent* directionalLightComponent = directionalLightEntity->getDirectionalLightComponent();
                    glm::vec4 colorAndIntensity = directionalLightComponent->getColorAndIntensity();
                    glm::vec3 direction = directionalLightComponent->getDirection();
                    sunLight.color = glm::vec3(colorAndIntensity.x, colorAndIntensity.y, colorAndIntensity.z);
                    sunLight.intensity = colorAndIntensity.w;
                    sunLight.direction = direction;
                }
            }
        }

        ~RaytracingScene() { }

        Camera camera;
        DirectionalLight sunLight;
        std::vector<Primitive> primitives;
        std::vector<Material> materials;
        std::vector<Instance> instances;
    };

    class CpuRaytracer
    {
    public:
        struct Ray
        {
            glm::vec3 ro;
            glm::vec3 rd;
        };

        struct HitRecord
        {
            f32 t;
            i32 instanceID;
        };

        struct SurfaceAttributes
        {
            glm::vec2 worldSpacePosition;
            glm::vec3 worldSpaceNormal;
            glm::vec3 albedo;
            f32 roughness;
            f32 metallic;
        };

        struct Image
        {
            i32 numPixels()
            {
                return width * height;
            }

            void clear(const glm::vec3& inClearColor)
            {
                for (i32 y = 0; y < height; ++y)
                {
                    for (i32 x = 0; x < width; ++x)
                    {
                        writePixel(glm::uvec2(x, y), clearColor);
                    }
                }
            }

            // return whether or not we resized
            bool resize(const glm::uvec2 newSize)
            {
                if (newSize.x != width || newSize.y != height)
                {
                    u32 numPixels = newSize.x * newSize.y;
                    pixels.resize(numPixels);
                    clear(clearColor);
                    width = newSize.x;
                    height = newSize.y;
                    return true;
                }
                return false;
            }

            const glm::vec3& readPixel(const glm::uvec2 pixelCoord) 
            { 
                u32 y = height - pixelCoord.y - 1;
                return pixels[y * width + pixelCoord.x]; 
            }

            void writePixel(const glm::uvec2& pixelCoord, glm::vec3 value) 
            {
                u32 y = height - pixelCoord.y - 1;
                pixels[y * width + pixelCoord.x] = value; 
            }
 
            i32 width = -1;
            i32 height = -1;
            glm::vec3 clearColor = glm::vec3(.2f);
            std::vector<glm::vec3> pixels;
        };

        struct ImageTile
        {
            glm::ivec2 start;
            glm::ivec2 end;
            i32 width;
            i32 height;
        };

        CpuRaytracer()
        {
            GfxTexture2D::Spec spec(1024, 1024, 1, PF_RGB16F);
            m_gpuTexture.reset(GfxTexture2D::create(spec, Sampler2D()));
            m_renderTarget.reset(createRenderTarget(spec.width, spec.height));
        }

        ~CpuRaytracer() { }

        // this function should run on main thread 
        void render(GfxTexture2D* outTexture)
        {
            // handle resizing
            bool bResize = (outTexture->width != m_gpuTexture->width) || (outTexture->height != m_gpuTexture->height);
            if (bResize)
            {
                GfxTexture2D::Spec spec(outTexture->width, outTexture->height, 1, PF_RGB16F);
                m_gpuTexture.reset(GfxTexture2D::create(spec, Sampler2D()));
                m_renderTarget.reset(createRenderTarget(spec.width, spec.height));
            }

            auto renderer = Renderer::get();

            // poll the completed tile queue to see if the outTexture can be updated
            std::unique_lock<std::mutex> lock(mutex);
            if (!m_tileQueue.empty())
            {
                ImageTile tile = m_tileQueue.front();
                m_tileQueue.pop();
                lock.unlock();

                // re-organize tile image memory
                std::vector<glm::vec3> tilePixels(tile.width * tile.height);
                for (i32 y = 0; y < tile.height; ++y)
                {
                    for (i32 x = 0; x < tile.width; ++x)
                    {
                        glm::vec3 pixel = m_image.readPixel(tile.start + glm::ivec2(x, y));
                        tilePixels[tile.width * y + x] = pixel;
                    }
                }
                // update m_gpuTexture's data
                glTextureSubImage2D(m_gpuTexture->getGpuResource(), 0, tile.start.x, tile.start.y, tile.width, tile.height, GL_RGB, GL_FLOAT, tilePixels.data());
            }
            else
            {
                lock.unlock();
            }

            m_renderTarget->setColorBuffer(outTexture, 0);
            m_renderTarget->setDrawBuffers({ 0 });
            m_renderTarget->clearDrawBuffer(0, glm::vec4(.0f, .0f, .0f, 1.f));

            // blit/copy m_gpuTexture to outTexture
            renderer->blitTexture(outTexture, m_gpuTexture.get());

            if (bRendering)
            {
                // todo: render a simple progress bar on top of the rendered image as long as the rendering is not finished
                glm::vec2 outerScale = glm::vec2(.8, .04);
                glm::vec2 innerScale = outerScale * glm::vec2(.99f, 0.95);
                glm::vec2 translation = glm::vec2(0.f, -0.8f);

                // progress bar background
                glm::vec2 outerMin = glm::vec2(-1.f * outerScale.x, -1.f * outerScale.y) + translation;
                glm::vec2 outerMax = glm::vec2( 1.f * outerScale.x,  1.f * outerScale.y) + translation;
                renderer->drawColoredScreenSpaceQuad(m_renderTarget.get(), outerMin * .5f + .5f, outerMax * .5f + .5f, glm::vec4(1.f));

                // actual bar
                f32 progress = renderedPixelCounter / (f32)m_image.numPixels();

                glm::vec2 innerMin = glm::vec2(-1.f * innerScale.x, -1.f * innerScale.y) + translation;
                glm::vec2 innerMax = glm::vec2( 1.f * innerScale.x * progress, 1.f * innerScale.y) + translation;
                renderer->drawColoredScreenSpaceQuad(m_renderTarget.get(), innerMin * .5f + .5f, innerMax * .5f + .5f, glm::vec4(0.f, 1.f, 0.f, 1.f));
            }
        }

        // this function should run on main thread but it dispatches work onto other threads
        void renderScene(const Scene& inScene, GfxTexture2D* raytracedOutput)
        {
            if (!bRendering)
            {
                bRendering = true;

                // construct scene
                RaytracingScene scene(inScene);

                // resize image if necessary
                m_image.resize(glm::uvec2(raytracedOutput->width, raytracedOutput->height));

                // clear image buffer and output gpu texture
                m_image.clear(glm::vec3(0.2f));
                glTextureSubImage2D(m_gpuTexture->getGpuResource(), 0, 0, 0, raytracedOutput->width, raytracedOutput->height, GL_RGB, GL_FLOAT, m_image.pixels.data());

                // clear progress counter
                renderedPixelCounter = 0;

                enum class TileRenderingOrder : u32
                {
                    kDown = 0,
                    kRight,
                    kUp,
                    kLeft,
                    kCount
                };

                std::thread thread([this]() {

                    const glm::ivec2 marchingDirections[(u32)TileRenderingOrder::kCount] = {
                        glm::ivec2(0, -1),
                        glm::ivec2(1,  0),
                        glm::ivec2(0,  1),
                        glm::ivec2(-1, 0),
                    };

                    // dispatch rendering tasks
                    // divide the output image into tiles
                    const glm::ivec2 tileSize(128, 128);
                    u32 tileCount = 0;

                    // number of tiles to march before direction switch follow the pattern of  1, 1, 2, 2, 3, 3, 4, 4, 5, 5 ...
                    u32 numDirectionSwitches = 0;
                    u32 tileCountInSingleDirectionToMarch = 1;
;                   u32 tileCountInSingleDirectionMarched = 0;

                    // pixel coord of bottom left corner of the starting tile
                    glm::ivec2 centerPixelCoord = glm::floor(glm::vec2(.5f) * glm::vec2(m_image.width, m_image.height));
                    glm::ivec2 start = centerPixelCoord - (tileSize / 2);
                    for (;;)
                    {
                        glm::ivec2 end = start + tileSize;

                        // sanitize start and end
                        glm::ivec2 sanitizedStart, sanitizedEnd;

                        sanitizedStart.x = max(0, start.x);
                        sanitizedStart.x = min(m_image.width, sanitizedStart.x);
                        sanitizedStart.y = max(0, start.y);
                        sanitizedStart.y = min(m_image.height, sanitizedStart.y);

                        sanitizedEnd.x = max(0, end.x);
                        sanitizedEnd.x = min(m_image.width, sanitizedEnd.x);
                        sanitizedEnd.y = max(0, end.y);
                        sanitizedEnd.y = min(m_image.height, sanitizedEnd.y);

                        // todo: stop condition seems wrong!!
                        // stop marching if a tile's start goes outside of the image, which means the rendering is finished
                        if (sanitizedEnd.x <= sanitizedStart.x && sanitizedEnd.y <= sanitizedEnd.y)
                        {
                            bRendering = false;
                            break;
                        }

                        // rendering one tile
                        for (i32 y = sanitizedStart.y; y < sanitizedEnd.y; ++y)
                        {
                            for (i32 x = sanitizedStart.x; x < sanitizedEnd.x; ++x)
                            {
#if 0
                                Ray ray = buildCameraRay(scene.camera, glm::uvec2(x, y));
                                HitRecord hitRecord = { };
                                if (trace(ray, scene, hitRecord))
                                {
                                    SurfaceAttributes sa = { };
                                    calcSurfaceAttributes(scene, hitRecord, sa);
                                    glm::vec3 outColor = shade(scene, sa);
                                }
#else // for debugging purpose
                                // glm::vec2 texCoord = glm::vec2(x, y) / glm::vec2(m_image.width, m_image.height);
                                glm::vec2 texCoord = (glm::vec2(x, y) - glm::vec2(start)) / glm::vec2(tileSize);
                                m_image.writePixel(glm::ivec2(x, y), glm::vec3(texCoord, 1.f));
#endif
                                renderedPixelCounter++;
                            }
                        }
                        // post message to the main rendering thread that a tile is rendered
                        ImageTile completedTile = { };
                        completedTile.start = sanitizedStart;
                        completedTile.end = sanitizedEnd;
                        completedTile.width = glm::abs(sanitizedEnd.x - sanitizedStart.x);
                        completedTile.height = glm::abs(sanitizedEnd.y - sanitizedStart.y);

                        std::unique_lock<std::mutex> lock(mutex);
                        m_tileQueue.push(completedTile);
                        lock.unlock();

                        // slow down the rendering
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));

                        // change direction
                        glm::ivec2 direction = marchingDirections[numDirectionSwitches % (u32)TileRenderingOrder::kCount];
                        // marching to the next tile
                        start = start + direction * glm::ivec2(tileSize);
                        tileCount++;

                        // update marching state
                        tileCountInSingleDirectionMarched += 1;
                        if (tileCountInSingleDirectionMarched == tileCountInSingleDirectionToMarch)
                        {
                            // reset currently marched tile counter in a single direction
                            tileCountInSingleDirectionMarched = 0;
                            numDirectionSwitches++;
                            // update tileCountInSingleDirectionToMarch if necessary
                            tileCountInSingleDirectionToMarch += numDirectionSwitches % 2 == 0 ? 1 : 0;
                        }
                    }
                });

                thread.detach();
            }
        }

    private:
        Ray buildCameraRay(const RaytracingScene::Camera& camera, const glm::uvec2& pixelCoord) { }

        void calcSurfaceAttributes(const RaytracingScene& scene, HitRecord& hitRecord, SurfaceAttributes& outAttribs)
        {
            // using barycentric coordinates interpolation to get per-pixel surface attributes
        }

        bool trace(const Ray& ray, const RaytracingScene& scene, HitRecord& outHit)
        {
        }

        glm::vec3 shade(const RaytracingScene& scene, const SurfaceAttributes& attributes)
        {
            glm::vec3 sceneColor(0.f);
            return sceneColor;
        }

        glm::vec3 postprocess()
        {
            glm::vec3 finalColor(0.f);
            return finalColor;
        }

        std::mutex mutex;
        std::queue<ImageTile> m_tileQueue;
        std::atomic<i32> renderedPixelCounter = 0;
        Image m_image;
        // gpu side copy of m_image
        std::unique_ptr<GfxTexture2D> m_gpuTexture = nullptr;
        std::unique_ptr<RenderTarget> m_renderTarget = nullptr;
        std::atomic<bool> bRendering = false;
    };
}

class App : public Cyan::DefaultApp
{
public:
    App() 
        : DefaultApp(1024, 1024)
    {
        m_cpuRaytracer = std::make_unique<Cyan::CpuRaytracer>();
        Cyan::GfxTexture2D::Spec spec(1024, 1024, 1, PF_RGB16F);
        m_raytracedOutput = std::unique_ptr<Cyan::GfxTexture2D>(Cyan::GfxTexture2D::create(spec, Cyan::Sampler2D()));
    }

    ~App() { }

    virtual void customInitialize() override
    {
        // building the scene

        // override rendering lambda
        m_renderOneFrame = [this](Cyan::GfxTexture2D* renderingOutput) {
            auto renderer = Cyan::Renderer::get();
            // normal scene rendering

            // render a progress bar and update renderingOutput when a tile has finished rendering
            m_cpuRaytracer->render(m_raytracedOutput.get());

            // UI rendering
            renderRaytracingViewport(m_raytracedOutput.get());
            renderer->renderUI();
        };
    }

    void renderRaytracingViewport(Cyan::GfxTexture2D* raytracedOutput)
    {
        auto renderer = Cyan::Renderer::get();
        renderer->addUIRenderCommand([this, raytracedOutput]() {
            ImGui::Begin("Raytracer Render View", nullptr, ImGuiWindowFlags_NoResize);
            {
                f32 totalHeight = ImGui::GetItemRectSize().y;
                ImGui::Text("Settings");
                totalHeight += ImGui::GetItemRectSize().y;
                if (ImGui::Button("Render Current View"))
                {
                    m_cpuRaytracer->renderScene(*m_scene, raytracedOutput);
                }
                totalHeight += ImGui::GetItemRectSize().y;
                ImGui::Separator();
                totalHeight += ImGui::GetItemRectSize().y;
                ImGui::Image((ImTextureID)raytracedOutput->getGpuResource(), ImVec2(raytracedOutput->width, raytracedOutput->height), ImVec2(0, 1), ImVec2(1, 0));
                totalHeight += ImGui::GetItemRectSize().y;
                ImGui::SetWindowSize("Raytracer Render View", ImVec2(1024.f, totalHeight + 30.f));
            }
            ImGui::End();
        });
    }

private:
    std::unique_ptr<Cyan::CpuRaytracer> m_cpuRaytracer = nullptr;
    std::unique_ptr<Cyan::GfxTexture2D> m_raytracedOutput = nullptr;
};

int main()
{
    auto app = std::make_unique<App>();
    app->run();
    return 0;
}
