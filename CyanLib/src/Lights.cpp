#include "Lights.h"

namespace Cyan {
    // helper function for converting world space AABB to light's view space
    BoundingBox3D calcLightSpaceAABB(const glm::vec3& inLightDirection, const BoundingBox3D& worldSpaceAABB)
    {
        // derive the 8 vertices of the aabb from pmin and pmax
        // d -- c
        // |    |
        // a -- b
        // f stands for front while b stands for back
        f32 width = worldSpaceAABB.pmax.x - worldSpaceAABB.pmin.x;
        f32 height = worldSpaceAABB.pmax.y - worldSpaceAABB.pmin.y;
        f32 depth = worldSpaceAABB.pmax.z - worldSpaceAABB.pmin.z;
        glm::vec3 fa = vec4ToVec3(worldSpaceAABB.pmax) + glm::vec3(-width, -height, 0.f);
        glm::vec3 fb = vec4ToVec3(worldSpaceAABB.pmax) + glm::vec3(0.f, -height, 0.f);
        glm::vec3 fc = vec4ToVec3(worldSpaceAABB.pmax);
        glm::vec3 fd = vec4ToVec3(worldSpaceAABB.pmax) + glm::vec3(-width, 0.f, 0.f);

        glm::vec3 ba = fa + glm::vec3(0.f, 0.f, -depth);
        glm::vec3 bb = fb + glm::vec3(0.f, 0.f, -depth);
        glm::vec3 bc = fc + glm::vec3(0.f, 0.f, -depth);
        glm::vec3 bd = fd + glm::vec3(0.f, 0.f, -depth);

        glm::mat4 lightSpaceView = glm::lookAt(glm::vec3(0.f), -inLightDirection, glm::vec3(0.f, 1.f, 0.f));
        BoundingBox3D lightSpaceAABB = { };
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(fa, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(fb, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(fc, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(fd, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(ba, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(bb, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(bc, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(bd, 1.f));
        return lightSpaceAABB;
    }

    void DirectionalLight::renderShadowMap(SceneRenderable& scene, Renderer* renderer) {
        BoundingBox3D lightSpaceAABB = calcLightSpaceAABB(direction, scene.aabb);
        shadowMap->render(lightSpaceAABB, scene, renderer);
    }

    void CSMDirectionalLight::renderShadowMap(SceneRenderable& scene, Renderer* renderer) {
        BoundingBox3D lightSpaceAABB = calcLightSpaceAABB(direction, scene.aabb);
        shadowMap->render(lightSpaceAABB, scene, renderer);
    }

    GpuCSMDirectionalLight CSMDirectionalLight::buildGpuLight() {
        GpuCSMDirectionalLight light = { };
        light.direction = glm::vec4(direction, 0.f);
        light.colorAndIntensity = colorAndIntensity;
        for (i32 i = 0; i < shadowMap->kNumCascades; ++i) {
            light.cascades[i].n = shadowMap->cascades[i].n;
            light.cascades[i].f = shadowMap->cascades[i].f;
            light.cascades[i].shadowMap.lightSpaceView = glm::lookAt(glm::vec3(0.f), -direction, glm::vec3(0.f, 1.f, 0.f));
            light.cascades[i].shadowMap.lightSpaceProjection = shadowMap->cascades[i].shadowMap->lightSpaceProjection;
            light.cascades[i].shadowMap.depthMapHandle = shadowMap->cascades[i].shadowMap->depthTexture->glHandle;
        }
        return light;
    }
}