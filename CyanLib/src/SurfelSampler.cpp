#pragma once

#include <set>

#include "SurfelSampler.h"
#include "Shader.h"
#include "Geometry.h"
#include "MathUtils.h"
#include "CyanRenderer.h"
#include "AssetManager.h"

namespace Cyan {
    static f32 calcSceneSurfaceArea(std::vector<SurfelSampler::Triangle>& outTriangles, const RenderableScene& inScene) {
        f32 totalArea = 0.f;
        for (i32 i = 0; i < inScene.meshInstances.size(); ++i) {
            auto meshInst = inScene.meshInstances[i];
            auto mesh = meshInst->mesh;
            glm::mat4 transform = (*inScene.transformBuffer)[i];
            glm::mat4 normalTransform = glm::inverse(glm::transpose(transform));
            for (i32 sm = 0; sm < mesh->numSubmeshes(); ++sm) {
                auto submesh = mesh->getSubmesh(sm);
                auto geometry = dynamic_cast<Triangles*>(submesh->geometry.get());
                if (geometry) 
                {
                    glm::vec3 albedo(0.f);
                    auto matl = meshInst->getMaterial(sm);
                    // todo: figure out how to handle textured material
                    if (matl) {
                        albedo = matl->albedo;
                    }
                    const auto& verts = geometry->vertices;
                    const auto& indices = geometry->indices;
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
                        totalArea += area;
                        // cache world space triangles for the next pass
                        outTriangles.emplace_back(
                            SurfelSampler::Triangle {
                                {p0, p1, p2},
                                {n0, n1, n2},
                                albedo,
                                area
                            }
                        );
                    }
                }
            }
        }
        return totalArea;
    }

    SurfelSampler::SurfelSampler() 
        : surfelSamples("SurfelBuffer")
        , instanceBuffer("InstanceBuffer")
    {

    }

    void SurfelSampler::sampleFixedNumberSurfels(std::vector<Surfel>& outSurfels, const RenderableScene& inScene) {
        // reset state
        surfelSamples.data.array.clear();

        std::vector<Triangle> triangles;
        f32 sceneSurfaceArea = calcSceneSurfaceArea(triangles, inScene);
        const u32 kNumSurfels = 64 * 64;
        // each triangle at least needs one surfel
        assert(kNumSurfels >= triangles.size());
        for (const auto& tri : triangles) {
            // face normal
            glm::vec3 tn = glm::normalize(glm::cross(
                glm::normalize(tri.p[1] - tri.p[0]),
                glm::normalize(tri.p[2] - tri.p[0])
            ));
            // transform the vertices from world space to tangent space
            glm::vec3 centroid = (tri.p[0] + tri.p[1] + tri.p[2]) / 3.f;
            glm::mat4 tangentToWorld = glm::translate(glm::mat4(1.f), centroid);
            glm::mat4 tangentFrame;
#if 0
            glm::vec3 t = glm::normalize(tri.p[0] - tri.p[1]);
            glm::vec3 b = glm::normalize(glm::cross(tn, t));
            tangentFrame[0] = glm::vec4(t, 0.f);
            tangentFrame[1] = glm::vec4(tn, 0.f);
            tangentFrame[2] = glm::vec4(b, 0.f);
            tangentFrame[3] = glm::vec4(0.f, 0.f, 0.f, 1.f);
#else
            tangentFrame = calcTangentFrame(tn);
#endif
            tangentToWorld = tangentToWorld * tangentFrame;
            glm::mat4 worldToTangent = glm::inverse(tangentToWorld);
            glm::vec3 v0 = worldToTangent * glm::vec4(tri.p[0], 1.f);
            glm::vec3 v1 = worldToTangent * glm::vec4(tri.p[1], 1.f);
            glm::vec3 v2 = worldToTangent * glm::vec4(tri.p[2], 1.f);
            // calculate the bounding rectangle on the tangent plane
            BoundingBox3D aabb;
            aabb.bound(v0);
            aabb.bound(v1);
            aabb.bound(v2);
            glm::vec3 origin(aabb.pmin.x, 0.f, aabb.pmax.z);
            f32 size = glm::max(abs(aabb.pmax.x - aabb.pmin.x), abs(aabb.pmax.z - aabb.pmin.z));
            u32 numSurfels = (u32)glm::max((tri.area / sceneSurfaceArea) * kNumSurfels, 1.f);
            i32 resolution = glm::sqrt((f32)numSurfels);
            // "software rasterize"
            for (i32 y = 0; y < resolution; ++y) {
                for (i32 x = 0; x < resolution; ++x) {
                    glm::vec2 pixelCoord(((f32)x + .5f) / resolution, ((f32)y + .5f) / resolution);
                    glm::vec3 tangentSpacePosition = origin + glm::vec3(pixelCoord.x * size, 0.f, -pixelCoord.y * size);
                    glm::vec3 worldSpacePosition = tangentToWorld * glm::vec4(tangentSpacePosition, 1.f);
#if 0
                    {
                        glm::mat4 transform = glm::translate(glm::mat4(1.f), worldSpacePosition);
                        transform = glm::scale(transform, glm::vec3(0.1f));
                        surfelSamples.addElement(transform);
                    }
                    {
                        glm::mat4 transform = glm::translate(glm::mat4(1.f), worldSpacePosition + tn * 0.01f);
                        transform = transform * calcTangentFrame(tn);
                        transform = glm::scale(transform, glm::vec3(size * 0.45f / resolution));
                        instanceBuffer.addElement(Instance{ transform, glm::vec4(0.f, 1.0f, 1.0f, 1.f) });
                    }
#endif
                }
            }
        }
        surfelSamples.upload();
        instanceBuffer.upload();
    }

    SurfelSampler::Tessellator::Tessellator(const Triangle& inTri) {
        tri = inTri;
        tn = glm::normalize(glm::cross(
            glm::normalize(tri.p[1] - tri.p[0]),
            glm::normalize(tri.p[2] - tri.p[0])
        ));
        assert(glm::length(tn) > 0.f);
        // transform the vertices from world space to tangent space
        glm::vec3 centroid = (tri.p[0] + tri.p[1] + tri.p[2]) / 3.f;
        glm::mat4 tangentFrame;
#if 0
        glm::vec3 t = glm::normalize(tri.p[0] - tri.p[1]);
        glm::vec3 b = glm::normalize(glm::cross(tn, t));
        tangentFrame[0] = glm::vec4(t, 0.f);
        tangentFrame[1] = glm::vec4(tn, 0.f);
        tangentFrame[2] = glm::vec4(b, 0.f);
        tangentFrame[3] = glm::vec4(0.f, 0.f, 0.f, 1.f);
#else
        tangentFrame = calcTangentFrame(tn);
#endif
        tangentToWorld = glm::translate(glm::mat4(1.f), centroid);
        tangentToWorld = tangentToWorld * tangentFrame;
        glm::mat4 worldToTangent = glm::inverse(tangentToWorld);
        glm::vec3 v0 = worldToTangent * glm::vec4(tri.p[0], 1.f);
        glm::vec3 v1 = worldToTangent * glm::vec4(tri.p[1], 1.f);
        glm::vec3 v2 = worldToTangent * glm::vec4(tri.p[2], 1.f);

        BoundingBox3D aabb;
        aabb.bound(v0);
        aabb.bound(v1);
        aabb.bound(v2);
        tangentPlaneBottomLeft = glm::vec3(aabb.pmin.x, 0.f, aabb.pmax.z);
        tangentPlaneTopRight = glm::vec3(aabb.pmax.x, 0.f, aabb.pmin.z);
        f32 width = tangentPlaneTopRight.x - tangentPlaneBottomLeft.x;
        f32 height = abs(tangentPlaneTopRight.z - tangentPlaneBottomLeft.z);
        assert(width > 0.f);
        assert(height > 0.f);
        resolution.x = ceil(width / (2.f * kTexelRadius));
        resolution.y = ceil(height / (2.f * kTexelRadius));
        size.x = resolution.x * (2.f * kTexelRadius);
        size.y = resolution.y * (2.f * kTexelRadius);
    }

    bool isInsideTriangle(const glm::vec3& p, const SurfelSampler::Triangle& tri, glm::vec3& outBarycentrics) {
        glm::vec3 tn = glm::normalize(glm::cross(
            glm::normalize(tri.p[1] - tri.p[0]),
            glm::normalize(tri.p[2] - tri.p[0])
        ));
        // inside-outside test & calculate barycentric
        const glm::vec3& v0 = tri.p[0];
        const glm::vec3& v1 = tri.p[1];
        const glm::vec3& v2 = tri.p[2];
        glm::vec3 u = glm::cross(v1 - v0, p - v0);
        glm::vec3 v = glm::cross(v2 - v1, p - v1);
        glm::vec3 w = glm::cross(v0 - v2, p - v2);
        bool isInside = (glm::dot(u, tn) >= 0.f && glm::dot(v, tn) >= 0.f && glm::dot(w, tn) >= 0.f);
        if (isInside) {
            f32 totalArea = glm::length(glm::cross(v1 - v0, v2 - v0));
            f32 ww = glm::length(w) / totalArea;
            f32 uu = glm::length(u) / totalArea;
            f32 vv = 1.f - (ww + uu);
            outBarycentrics = glm::vec3(uu, vv, ww);
        }
        return isInside;
    }

    void SurfelSampler::Tessellator::tessellate(std::vector<Surfel>& outSurfels) {
        // "software rasterize"
        for (i32 y = 0; y < resolution.y; ++y) {
            for (i32 x = 0; x < resolution.x; ++x) {
                // pixel center
                glm::vec2 pixelCoord(((f32)x + .5f) / resolution.x, ((f32)y + .5f) / resolution.y);
                glm::vec3 tangentSpacePosition = tangentPlaneBottomLeft + glm::vec3(pixelCoord.x * size.x, 0.f, -pixelCoord.y * size.y);
                glm::vec3 worldSpacePosition = tangentToWorld * glm::vec4(tangentSpacePosition, 1.f);
                glm::vec3 barycentrics;
                if (isInsideTriangle(worldSpacePosition, tri, barycentrics)) {
                    glm::vec3 n = barycentrics.x * tri.vn[0] + barycentrics.y * tri.vn[1] + barycentrics.z * tri.vn[2];
                    glm::vec3 albedo = tri.albedo;
                    samplePoints.push_back(Sample{
                            worldSpacePosition,
                            n,
                            albedo
                    });
                    outSurfels.push_back(Surfel{
                            worldSpacePosition,
                            n,
                            tri.albedo,
                            kTexelRadius * glm::sqrt(2.f)
                    });
                }
                else {
                    // todo: subdivide partially covered "pixel" once to suppress aliasing around triangle edges
                    /** note - @min:
                    * Resolving holes around the edges of triangles by doing conservative rasterization but this 
                    * introduces duplicated surfels being generated, not sure if these duplicates will cause issues
                    * later when building surfel bounding hierarchy, so leaving a note here.
                    */
                    // conservative rasterization
                    bool bPartiallyCovered = false;
                    // four corners of a pixel
                    enum class PixelCorner : u32 {
                        kBottomLeft = 0,
                        kBottomRight,
                        kTopLeft,
                        kTopRight,
                        kCount
                    };
                    glm::vec2 pixelCorners[(u32)PixelCorner::kCount] = {
                        glm::vec2((f32)x / resolution.x, (f32)y / resolution.y),
                        glm::vec2(((f32)x + 1.f) / resolution.x, (f32)y / resolution.y),
                        glm::vec2((f32)x / resolution.x, ((f32)y + 1.f) / resolution.y),
                        glm::vec2(((f32)x + 1.f) / resolution.x, ((f32)y + 1.f) / resolution.y),
                    };
                    for (u32 i = 0; i < (u32)PixelCorner::kCount; ++i) {
                        glm::vec2 pixelCoord = pixelCorners[i];
                        glm::vec3 tangentSpacePosition = tangentPlaneBottomLeft + glm::vec3(pixelCoord.x * size.x, 0.f, -pixelCoord.y * size.y);
                        glm::vec3 worldSpacePosition = tangentToWorld * glm::vec4(tangentSpacePosition, 1.f);
                        glm::vec3 barycentrics;
                        if (isInsideTriangle(worldSpacePosition, tri, barycentrics)) {
                            bPartiallyCovered = true;
                            break;
                        }
                    }
                    // todo: can use coverage to attenuate the color / lighting contribution for partially covered surfels  
                    if (bPartiallyCovered) {
                        outSurfels.push_back(Surfel{
                                worldSpacePosition,
                                tn,
                                tri.albedo,
                                kTexelRadius * glm::sqrt(2.f)
                        });
                        samplePoints.push_back(Sample{
                            worldSpacePosition,
                            tn,
                            tri.albedo
                        });
                    }
                }
            }
        }
    }

    /** note - @min:
    * instead of fixing the number of surfels, fix the surfel's radius
    */
    void SurfelSampler::sampleFixedSizeSurfels(std::vector<Surfel>& outSurfels, const RenderableScene& inScene) {
        // reset state
        surfelSamples.data.array.clear();
        instanceBuffer.data.array.clear();

        std::vector<Triangle> triangles;
        f32 sceneSurfaceArea = calcSceneSurfaceArea(triangles, inScene);
        static const f32 kSurfelRadius = 0.1f;
        for (i32 i = 0; i < triangles.size(); ++i) {
            Tessellator tessellator(triangles[i]);
            tessellator.tessellate(outSurfels);

            // for visualization
            for (const auto& sample : tessellator.samplePoints) {
                {
                    glm::mat4 transform = glm::translate(glm::mat4(1.f), sample.p);
                    // transform *= calcTangentFrame(sample.n);
                    transform = glm::scale(transform, glm::vec3(tessellator.kTexelRadius));
                    surfelSamples.addElement(Instance{ transform, glm::vec4(sample.color, 1.f)
                    });
                }
                {
                    glm::mat4 transform = glm::translate(glm::mat4(1.f), sample.p + tessellator.tn * 0.01f);
                    transform = transform * calcTangentFrame(tessellator.tn);
                    transform = glm::scale(transform, glm::vec3(tessellator.kTexelRadius));
                    instanceBuffer.addElement(Instance{ transform, glm::vec4(0.f, 1.0f, 1.0f, 1.f) });
                }
            }
        }
        surfelSamples.upload();
        instanceBuffer.upload();

        // deduplicate really close surfels
        // deduplicate(outSurfels);
    }

    // todo: speed up this!
    void SurfelSampler::deduplicate(std::vector<Surfel>& outSurfels) {
        std::set<u32> duplicatedSet;
        i32 numSurfelsBefore = outSurfels.size();
        const f32 kEpsilon = 0.1;
        for (i32 i = 0; i < outSurfels.size(); ++i) {
            for (i32 j = i + 1; j < outSurfels.size(); ++j) {
                f32 dist = glm::distance(outSurfels[i].position, outSurfels[j].position);
                // todo: also need to take normal into account
                if (dist <= kEpsilon) {
                    duplicatedSet.insert(j);
                }
            }
        }
        std::vector<Surfel> deduplicated;
        for (i32 i = 0; i < outSurfels.size(); ++i) {
            if (duplicatedSet.find(i) == duplicatedSet.end()) {
                deduplicated.push_back(outSurfels[i]);
            }
        }
        outSurfels.swap(deduplicated);
        i32 numSurfelsAfter = outSurfels.size();
        i32 diff = numSurfelsBefore - numSurfelsAfter;
        cyanInfo("Deduplicated %d surfels", diff);
    }

    void SurfelSampler::visualize(RenderTarget* dstRenderTarget, Renderer* renderer) 
    {
        auto gfxc = renderer->getGfxCtx();
        gfxc->setRenderTarget(dstRenderTarget);
        gfxc->setViewport({ 0, 0, dstRenderTarget->width, dstRenderTarget->height });
        gfxc->setDepthControl(DepthControl::kEnable);
        // visualize sample points
        {
            surfelSamples.bind(80);
            CreateVS(vs, "DebugDrawSamplePointVS", SHADER_SOURCE_PATH "debug_draw_sample_points_v.glsl");
            CreatePS(ps, "DebugDrawSamplePointPS", SHADER_SOURCE_PATH "debug_draw_sample_points_p.glsl");
            CreatePixelPipeline(pipeline, "DebugDrawSamplePoint", vs, ps);
            gfxc->setPixelPipeline(pipeline, [](VertexShader* vs, PixelShader* ps) {

            });
            auto sphere = AssetManager::getAsset<StaticMesh>("Sphere");
            auto sm = sphere->getSubmesh(0);
            auto va = sm->getVertexArray();
            gfxc->setVertexArray(va);
            i32 instanceCount = surfelSamples.getNumElements();
            glDrawElementsInstanced(GL_TRIANGLES, sm->numIndices(), GL_UNSIGNED_INT, nullptr, instanceCount);
        }
        // visualize grid
        {
            instanceBuffer.bind(81);
            CreateVS(vs, "DebugDrawSurfelGridVS", SHADER_SOURCE_PATH "debug_draw_grid_v.glsl");
            CreatePS(ps, "DebugDrawSurfelGridPS", SHADER_SOURCE_PATH "debug_draw_grid_p.glsl");
            CreatePixelPipeline(pipeline, "DebugDrawSurfelGrid", vs, ps);
            gfxc->setPixelPipeline(pipeline);
            auto quad = AssetManager::getAsset<StaticMesh>("Quad");
            auto sm = quad->getSubmesh(0);
            auto va = sm->getVertexArray();
            gfxc->setVertexArray(va);
            u32 instanceCount = instanceBuffer.getNumElements();
            glDrawElementsInstanced(GL_TRIANGLES, sm->numIndices(), GL_UNSIGNED_INT, nullptr, instanceCount);
        }
    }
}
