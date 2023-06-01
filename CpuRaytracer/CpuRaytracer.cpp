#include <algorithm>

#include "glm.hpp"
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_internal.h>
#include <AssetImporter.h>
#include <AssetManager.h>

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
            glm::vec3 m_lookAt;
            glm::vec3 forward;
            glm::vec3 right;
            glm::vec3 up;
            f32 fov;
            f32 aspect;
            f32 n;
            f32 f;
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
                camera.eye = inCamera->m_position;
                camera.m_lookAt = inCamera->m_lookAt;
                camera.aspect = inCamera->aspectRatio;
                camera.fov = inCamera->fov;
                camera.forward = inCamera->forward();
                camera.right = inCamera->right();
                camera.up = inCamera->up();
                camera.n = inCamera->n;
                camera.f = inCamera->f;
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
                    // todo: found a bug here, mesh doesn't have a name
                    auto meshEntry = meshMap.find(mesh->m_name);

                    if (meshEntry == meshMap.end())
                    {
                        i32 basePrimitiveID = primitives.size();
                        for (i32 i = 0; i < mesh->numSubmeshes(); ++i)
                        {
                            Instance instance = { };
                            instance.localToWorld = staticMeshEntity->getWorldTransformMatrix();
                            instance.primitiveID = basePrimitiveID + i;
                            instances.push_back(instance);

                            auto sm = mesh->getSubmesh(i);
                            if (auto triangles = dynamic_cast<Triangles*>(sm->geometry))
                            {
                                primitives.emplace_back();

                                Primitive& primitive = primitives.back();
                                primitive.positions.resize(triangles->numVertices());
                                primitive.normals.resize(triangles->numVertices());
                                primitive.indices = triangles->indices;

                                // fill in geometry data
                                for (i32 v = 0; v < triangles->numVertices(); ++v)
                                {
                                    const Triangles::Vertex& vertex = triangles->vertices[v];
                                    primitive.positions[v] = vertex.pos;
                                    primitive.normals[v] = vertex.normal;
                                }
                            }
                        }
                        meshMap.insert({ mesh->m_name, basePrimitiveID });
                    }
                    else
                    {
                        u32 basePrimitiveID = meshEntry->second;
                        for (i32 i = 0; i < mesh->numSubmeshes(); ++i)
                        {
                            Instance instance = { };
                            instance.localToWorld = staticMeshEntity->getWorldTransformMatrix();
                            instance.primitiveID = basePrimitiveID + i;
                            instances.push_back(instance);
                        }
                    }

                    for (i32 i = 0; i < mesh->numSubmeshes(); ++i)
                    {
                        auto material = meshInstance->getMaterial(i);
                        auto materialEntry = materialMap.find(material->m_name);
                        if (materialEntry == materialMap.end())
                        {

                        }
                        else
                        {

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
        std::vector<Material> m_materials;
        std::vector<Instance> instances;
        std::vector<glm::mat4> transforms;
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
            i32 triangleID;
        };

        struct SurfaceAttributes
        {
            glm::vec3 worldSpacePosition;
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
            m_framebuffer.reset(createFramebuffer(spec.width, spec.height));
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
                m_framebuffer.reset(createFramebuffer(spec.width, spec.height));
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

            m_framebuffer->setColorBuffer(outTexture, 0);
            m_framebuffer->setDrawBuffers({ 0 });
            m_framebuffer->clearDrawBuffer(0, glm::vec4(.0f, .0f, .0f, 1.f));

            // blit/copy m_gpuTexture to outTexture
            renderer->blitTexture(outTexture, m_gpuTexture.get());

            if (bRendering)
            {
                glm::vec2 outerScale = glm::vec2(.8, .04);
                glm::vec2 innerScale = outerScale * glm::vec2(.99f, 0.95);
                glm::vec2 translation = glm::vec2(0.f, -0.8f);

                // progress bar background
                glm::vec2 outerMin = glm::vec2(-1.f * outerScale.x, -1.f * outerScale.y) + translation;
                glm::vec2 outerMax = glm::vec2( 1.f * outerScale.x,  1.f * outerScale.y) + translation;
                renderer->drawColoredScreenSpaceQuad(m_framebuffer.get(), outerMin * .5f + .5f, outerMax * .5f + .5f, glm::vec4(1.f));

                // actual bar
                f32 progress = renderedPixelCounter / (f32)m_image.numPixels();

                glm::vec2 innerMin = glm::vec2(-1.f * innerScale.x, -1.f * innerScale.y) + translation;
                glm::vec2 innerMax = glm::vec2((-1.f + progress * 2.f) * innerScale.x, 1.f * innerScale.y) + translation;
                renderer->drawColoredScreenSpaceQuad(m_framebuffer.get(), innerMin * .5f + .5f, innerMax * .5f + .5f, glm::vec4(0.f, 1.f, 0.f, 1.f));
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

                std::thread thread([this, scene]() {

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

                        sanitizedStart.x = Max(0, start.x);
                        sanitizedStart.x = Min(m_image.width, sanitizedStart.x);
                        sanitizedStart.y = Max(0, start.y);
                        sanitizedStart.y = Min(m_image.height, sanitizedStart.y);

                        sanitizedEnd.x = Max(0, end.x);
                        sanitizedEnd.x = Min(m_image.width, sanitizedEnd.x);
                        sanitizedEnd.y = Max(0, end.y);
                        sanitizedEnd.y = Min(m_image.height, sanitizedEnd.y);

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
#if 1
                                glm::vec3 sceneColor = glm::vec3(.15f);
                                Ray ray = buildCameraRay(scene.camera, glm::uvec2(x, y), glm::uvec2(m_image.width, m_image.height));
                                HitRecord hitRecord = { };
                                if (trace(ray, scene, hitRecord))
                                {
                                    SurfaceAttributes sa = { };
                                    calcSurfaceAttributes(ray, scene, hitRecord, sa);
                                    sceneColor = shade(scene, sa);
                                }
                                glm::vec3 finalColor = postprocess(sceneColor);
                                m_image.writePixel(glm::ivec2(x, y), finalColor);
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
        Ray buildCameraRay(const RaytracingScene::Camera& camera, const glm::uvec2& pixelCoord, const glm::uvec2& imageSize)
        { 
            glm::vec2 uv = (glm::vec2(pixelCoord) + .5f) / glm::vec2(imageSize) * 2.f - 1.f;
            /** note:
                * I was used to calculate ray direction using camera.right * uv.x + camera.up * uv.y + camera.forward * n as I thought the image plane should be
                displaced n away from the camera. However, fixing uv while tweaking n is actually changing the fov, leading to rendered image having a mismatched
                fov compared to rasterized view.
             */
            // calculate the distance between image plane and camera given camera fov
            f32 d = 1.f / glm::tan(glm::radians(camera.fov * .5f));
            glm::vec3 rd = glm::normalize(camera.right * uv.x + camera.up * uv.y + camera.forward * d);
            return Ray{ camera.eye, rd };
        }

        glm::vec3 calcBarycentrics(const glm::vec3& p, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2)
        {
            f32 totalArea = glm::length(glm::cross(v1 - v0, v2 - v0));

            f32 w = glm::length(glm::cross(v1 - v0, p - v0)) / totalArea;
            f32 v = glm::length(glm::cross(v2 - v0, p - v0)) / totalArea;
            f32 u = 1.f - v - w;
            return glm::vec3(u, v, w);
        }

        void calcSurfaceAttributes(const Ray& ray, const RaytracingScene& scene, HitRecord& hitRecord, SurfaceAttributes& outAttribs)
        {
            if (hitRecord.instanceID >= 0)
            {
                // using barycentric coordinates interpolation to get per-pixel surface attributes
                const RaytracingScene::Instance& instance = scene.instances[hitRecord.instanceID];
                const RaytracingScene::Primitive& primitive = scene.primitives[instance.primitiveID];
                glm::vec3 p = ray.ro + hitRecord.t * ray.rd;
                i32 i0 = primitive.indices[hitRecord.triangleID * 3 + 0];
                i32 i1 = primitive.indices[hitRecord.triangleID * 3 + 1];
                i32 i2 = primitive.indices[hitRecord.triangleID * 3 + 2];
                glm::vec3 v0 = primitive.positions[i0];
                glm::vec3 v1 = primitive.positions[i1];
                glm::vec3 v2 = primitive.positions[i2];
                glm::vec3 n0 = primitive.normals[i0];
                glm::vec3 n1 = primitive.normals[i1];
                glm::vec3 n2 = primitive.normals[i2];
                glm::vec3 barycentrics = calcBarycentrics(p, v0, v1, v2);
                glm::vec3 localPosition = v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
                glm::vec3 localNormal = glm::normalize(n0 * barycentrics.x + n1 * barycentrics.y + n2 * barycentrics.z);

                outAttribs.worldSpacePosition = ray.ro + hitRecord.t * ray.rd;
                outAttribs.worldSpaceNormal = glm::normalize(glm::inverse(glm::transpose(instance.localToWorld)) * glm::vec4(localNormal, 0.f));
                if (instance.materialID >= 0)
                {
                    outAttribs.albedo = scene.m_materials[instance.materialID].albedo;
                    outAttribs.metallic = scene.m_materials[instance.materialID].metallic;
                    outAttribs.roughness = scene.m_materials[instance.materialID].roughness;
                }
                else
                {
                    outAttribs.albedo = glm::vec3(.8f);
                    outAttribs.metallic = 0.f;
                    outAttribs.roughness = .5f;
                }
            }
        }

        // taken from https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
        f32 intersect(const Ray& ray, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2)
        {
            const float EPSILON = 0.0000001;
            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;
            glm::vec3 h, s, q;
            float a,f,u,v;
            h = glm::cross(ray.rd, edge2);
            // a = glm::dot(edge1, h);
            a = edge1.x * h.x + edge1.y * h.y + edge1.z * h.z;
            if (fabs(a) < EPSILON)
            {
                return -1.0f;
            }
            f = 1.0f / a;
            s = ray.ro - v0;
            // u = f * dot(s, h);
            u = f * (s.x*h.x + s.y*h.y + s.z*h.z);
            if (u < 0.0 || u > 1.0)
            {
                return -1.0;
            }
            q = glm::cross(s, edge1);
            v = f * (ray.rd.x*q.x + ray.rd.y*q.y + ray.rd.z*q.z);
            if (v < 0.0 || u + v > 1.0)
            {
                return -1.0;
            }
            float t = f * glm::dot(edge2, q);
            // hit
            if (t > EPSILON)
            {
                return t;
            }
            return -1.0f;
        }

        bool traceShadow(const Ray& ray, const RaytracingScene& scene)
        {
            bool bAnyHit = false;

            // brute-force tracing for now
            for (const auto& instance : scene.instances)
            {
                // transform ray to object space
                Ray localRay = { ray.ro, ray.rd };
                glm::mat4 worldToLocal = glm::inverse(instance.localToWorld);
                localRay.ro = worldToLocal * glm::vec4(ray.ro, 1.f);
                localRay.rd = worldToLocal * glm::vec4(ray.rd, 0.f);

                const RaytracingScene::Primitive& p = scene.primitives[instance.primitiveID];
                assert(p.indices.size() % 3 == 0);
                u32 numTriangles = p.indices.size() / 3;
                for (i32 tri = 0; tri < numTriangles; ++tri)
                {
                    glm::vec3 v0 = p.positions[p.indices[tri * 3 + 0]];
                    glm::vec3 v1 = p.positions[p.indices[tri * 3 + 1]];
                    glm::vec3 v2 = p.positions[p.indices[tri * 3 + 2]];
                    f32 localT = intersect(localRay, v0, v1, v2);
                    if (localT >= 0.f)
                    {
                        bAnyHit = true;
                        break;
                    }
                }
            }

            return bAnyHit;
        }

        bool trace(const Ray& ray, const RaytracingScene& scene, HitRecord& outHit)
        {
            outHit.instanceID = -1;
            outHit.triangleID = -1;

            f32 closestWorldT = FLT_MAX;
            // brute-force tracing for now
            i32 instanceID = 0;
            for (const auto& instance : scene.instances)
            {
                // transform ray to object space
                Ray localRay = { ray.ro, ray.rd };
                glm::mat4 worldToLocal = glm::inverse(instance.localToWorld);
                localRay.ro = worldToLocal * glm::vec4(ray.ro, 1.f);
                localRay.rd = worldToLocal * glm::vec4(ray.rd, 0.f);

                const RaytracingScene::Primitive& p = scene.primitives[instance.primitiveID];
                assert(p.indices.size() % 3 == 0);
                u32 numTriangles = p.indices.size() / 3;
                i32 triangleID = -1;
                f32 closesLocalT = FLT_MAX;
                for (i32 tri = 0; tri < numTriangles; ++tri)
                {
                    glm::vec3 v0 = p.positions[p.indices[tri * 3 + 0]];
                    glm::vec3 v1 = p.positions[p.indices[tri * 3 + 1]];
                    glm::vec3 v2 = p.positions[p.indices[tri * 3 + 2]];
                    f32 localT = intersect(localRay, v0, v1, v2);
                    if (localT >= 0.f && localT < closesLocalT)
                    {
                        closesLocalT = localT;
                        triangleID = tri;
                    }
                }
                if (triangleID >= 0)
                {
                    // transform closest local hit position back to world space and compared with world space hit position
                    glm::vec3 localHitPosition = localRay.ro + closesLocalT * localRay.rd;
                    glm::vec3 worldSpaceHitPos = instance.localToWorld * glm::vec4(localHitPosition, 1.f);
                    f32 worldT = (worldSpaceHitPos.x - ray.ro.x) / ray.rd.x;
                    if (worldT < closestWorldT)
                    {
                        closestWorldT = worldT;

                        outHit.instanceID = instanceID;
                        outHit.triangleID = triangleID;
                        outHit.t = worldT;
                    }
                }

                instanceID++;
            }
            return outHit.instanceID >= 0;
        }

#define RAY_BUMP 0.001f

        f32 calcAO(const RaytracingScene& scene, const SurfaceAttributes& attributes)
        { 
            const i32 kNumSamples = 16;
            f32 ao = 0.f;
            for (i32 i = 0; i < kNumSamples; ++i)
            {
                Ray ray = { };
                ray.ro = attributes.worldSpacePosition + attributes.worldSpaceNormal * RAY_BUMP;
                ray.rd = cosineWeightedSampleHemisphere(attributes.worldSpaceNormal);
                ao += traceShadow(ray, scene) ? 0.f : 1.f;
            }
            return ao / (f32)kNumSamples;
        }

        glm::vec3 calcDirectLighting(const RaytracingScene& scene, const SurfaceAttributes& attributes)
        {
            glm::vec3 outColor(0.f);

            const auto& sunLight = scene.sunLight;
            glm::vec3 li = sunLight.color * sunLight.intensity;
            f32 ndotl = glm::max(glm::dot(sunLight.direction, attributes.worldSpaceNormal), 0.f);

            Ray shadowRay = { };
            shadowRay.ro = attributes.worldSpacePosition + attributes.worldSpaceNormal * RAY_BUMP;
            shadowRay.rd = sunLight.direction;
            f32 shadow = traceShadow(shadowRay, scene) ? 0.f : 1.f;
            outColor += (li * shadow) * attributes.albedo / glm::pi<f32>() * ndotl;

#if 0
            const glm::vec3 ambient(0.529f, 0.808f, 0.922f);
            outColor += ambient * calcAO(scene, attributes);
#endif
            // todo: direct skylight using HDR

            return outColor;
        }

        glm::vec3 calcIndirectLighting(const RaytracingScene& scene, const SurfaceAttributes& attributes)
        {
            const i32 kNumDiffuseSamples = 64u;

            glm::vec3 irradiance(0.f);
            // one diffuse indirect bounce
            for (i32 i = 0; i < kNumDiffuseSamples; ++i)
            {
                Ray ray = { };
                ray.ro = attributes.worldSpacePosition + attributes.worldSpaceNormal * RAY_BUMP;
                ray.rd = cosineWeightedSampleHemisphere(attributes.worldSpaceNormal);
                HitRecord outHit;
                if (trace(ray, scene, outHit))
                {
                    SurfaceAttributes sa;
                    calcSurfaceAttributes(ray, scene, outHit, sa);
                    glm::vec3 radiance = calcDirectLighting(scene, sa);
                    irradiance += radiance;
                }
            }
            irradiance /= (f32)kNumDiffuseSamples;
            return irradiance;
        }

        glm::vec3 shade(const RaytracingScene& scene, const SurfaceAttributes& attributes)
        {
            glm::vec3 sceneColor(0.f);
            sceneColor += calcDirectLighting(scene, attributes);
            sceneColor += calcIndirectLighting(scene, attributes);
            return sceneColor;
        }

        glm::vec3 postprocess(const glm::vec3& inColor)
        {
            glm::vec3 finalColor = inColor;
            // simple tonemapping
            finalColor /= finalColor + 1.f;
            finalColor = glm::smoothstep(0.f, 1.f, finalColor);
            // gamma
            finalColor = glm::vec3(glm::pow(finalColor.x, 0.4545f), glm::pow(finalColor.y, .4545f), glm::pow(finalColor.z, .4545f));
            return finalColor;
        }

        std::mutex mutex;
        std::queue<ImageTile> m_tileQueue;
        std::atomic<i32> renderedPixelCounter = 0;
        Image m_image;
        // gpu side copy of m_image
        std::unique_ptr<GfxTexture2D> m_gpuTexture = nullptr;
        std::unique_ptr<Framebuffer> m_framebuffer = nullptr;
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
        Cyan::AssetImporter::importAsync(m_scene.get(), ASSET_PATH "mesh/raytracing_test_scene.glb");
        // sun light
        m_scene->createDirectionalLight("SunLight", glm::vec3(0.8f, 1.3f, 0.5f), glm::vec4(0.88f, 0.77f, 0.65f, 10.f));

        // override rendering lambda
        m_renderOneFrame = [this](Cyan::GfxTexture2D* renderingOutput) {
            auto renderer = Cyan::Renderer::get();
            // normal scene rendering
            if (m_scene)
            {
                if (auto camera = dynamic_cast<Cyan::PerspectiveCamera*>(m_scene->m_mainCamera->getCamera())) 
                {
                    Cyan::SceneView mainSceneView(*m_scene, *camera,
                        [](Cyan::Entity* entity) {
                            return entity->getProperties() | EntityFlag_kVisible;
                        },
                        renderingOutput, 
                        { 0, 0, renderingOutput->width, renderingOutput->height }
                    );
                    renderer->render(m_scene.get(), mainSceneView, glm::uvec2(renderingOutput->width, renderingOutput->height));
                }
            }

            // render a progress bar and update renderingOutput when a tile has finished rendering
            m_cpuRaytracer->render(m_raytracedOutput.get());

            // UI rendering
            renderApp(renderingOutput, m_raytracedOutput.get());
            renderer->renderUI();
        };
    }
    
    void renderApp(Cyan::GfxTexture2D* renderingOutput, Cyan::GfxTexture2D* raytracedOutput)
    {
        auto renderer = Cyan::Renderer::get();

        renderer->addUIRenderCommand([this, renderingOutput]() {
            ImGui::DockSpaceOverViewport();
            auto flags = ImGuiWindowFlags_NoBackground;
            ImGui::Begin("App", nullptr, flags);
            {
                ImGui::Image((ImTextureID)renderingOutput->getGpuResource(), ImGui::GetContentRegionAvail(), ImVec2(0, 1), ImVec2(1, 0));
            }
            ImGui::End();
        });

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
                ImVec2 imageSize = ImGui::GetContentRegionAvail();
                ImGui::Image((ImTextureID)raytracedOutput->getGpuResource(), imageSize, ImVec2(0, 1), ImVec2(1, 0));
                totalHeight += ImGui::GetItemRectSize().y;
                ImGui::SetWindowSize("Raytracer Render View", ImVec2(1024.f, totalHeight + 30.f));
            }
            ImGui::End();
        });
    }

    void renderRaytracingViewport(Cyan::GfxTexture2D* raytracedOutput)
    {
        auto renderer = Cyan::Renderer::get();
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
