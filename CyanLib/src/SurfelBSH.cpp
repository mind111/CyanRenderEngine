#include "SurfelBSH.h"
#include "Geometry.h"
#include "glm/common.hpp"
#include "Mesh.h"
#include "GfxContext.h"
#include "AssetManager.h"
#include "RenderTarget.h"

namespace Cyan {
    void SurfelBSH::Node::build(const std::vector<Surfel>& surfels, u32 surfelIndex) {
        // this node is a child node
        surfelID = surfelIndex;
        center = surfels[surfelID].position;
        radius = surfels[surfelID].radius;
        albedo = surfels[surfelID].albedo;
        normal = surfels[surfelID].normal;
    }

    void SurfelBSH::Node::build(const std::vector<Node>& nodes, u32 leftChildIndex, u32 rightChildIndex) {
        const auto& leftChild = nodes[leftChildIndex];
        const auto& rightChild = nodes[rightChildIndex];
        // this node is a internal node
        center = (leftChild.center + rightChild.center) * .5f;
        radius = glm::max(
            glm::distance(center, leftChild.center) + leftChild.radius,
            glm::distance(center, rightChild.center) + rightChild.radius
        );
        normal = glm::normalize(leftChild.normal + rightChild.normal);
        albedo = (leftChild.albedo + rightChild.albedo) * .5f;
        coneAperture = glm::max(
            glm::acos(glm::dot(leftChild.normal, normal)),
            glm::acos(glm::dot(rightChild.normal, normal))
        );
        childs[0] = leftChildIndex;
        childs[1] = rightChildIndex;
    }

    void SurfelBSH::build(const std::vector<Surfel>& inSurfels) {
        surfels = inSurfels;

        u32 numSurfels = surfels.size();
        buildInternal(allocNode(), surfels, 0, numSurfels);
        numLevels = levelFirstTraversal(0, -1);
        // todo: validate built BSH
    }

    u32 SurfelBSH::allocNode() {
        u32 nodeIndex = nodes.size();
        nodes.emplace_back();
        return nodeIndex;
    }

    i32 SurfelBSH::levelFirstTraversal(i32 startLevel, i32 numLevelsToTraverse, const std::function<void(i32)>& callback) {
        u32 level = 0;
        std::queue<i32> currentLevel;
        std::queue<i32> nextLevel;
        currentLevel.push(0);
        while (!currentLevel.empty()) {
            if ((numLevelsToTraverse >= 0) && (level >= (startLevel + numLevelsToTraverse))) {
                break;
            }
            i32 nodeIndex = currentLevel.front();
            i32 leftChildIndex = nodes[nodeIndex].childs[0];
            i32 rightChildIndex = nodes[nodeIndex].childs[1];
            if (leftChildIndex >= 0) {
                nextLevel.push(leftChildIndex);
            }
            if (rightChildIndex >= 0) {
                nextLevel.push(rightChildIndex);
            }
            if (level >= startLevel && level < (startLevel + numLevelsToTraverse)) {
                callback(nodeIndex);
            }
            currentLevel.pop();
            if (currentLevel.empty()) {
                currentLevel.swap(nextLevel);
                level++;
            }
        }
        if (numLevelsToTraverse < 0) {
            numLevelsToTraverse = level;
        }
        return numLevelsToTraverse;
    }

    void SurfelBSH::buildInternal(i32 nodeIndex, std::vector<Surfel>& surfels, u32 start, u32 end) {
        // empty leaf node
        if (end - start == 0) {
            return;
        }
        // leaf node
        else if ((end - start) == 1u) { 
            nodes[nodeIndex].build(surfels, start);
        }
        else if ((end - start) > 1u) {
            // determine dominant axis
            BoundingBox3D aabb;
            for (u32 i = 0; i < (end - start); ++i) {
                aabb.bound(surfels[start + i].position);
            }
            f32 extentX = abs(aabb.pmax.x - aabb.pmin.x);
            f32 extentY = abs(aabb.pmax.y - aabb.pmin.y);
            f32 extentZ = abs(aabb.pmax.z - aabb.pmin.z);
            // sort in dominant axis
            const auto& beginItr = surfels.begin() + start;
            const auto& endItr = surfels.begin() + end;
            enum class SplitAxis {
                kX = 0,
                kY,
                kZ,
                kInvalid
            } splitAxis = SplitAxis::kInvalid;
            if (extentX >= glm::max(extentY, extentZ)) {
                std::sort(beginItr, endItr,
                    [](const Surfel& a, const Surfel& b) {
                        return a.position.x < b.position.x;
                    });
                splitAxis = SplitAxis::kX;
            }
            else if (extentY >= glm::max(extentX, extentZ)) {
                std::sort(beginItr, endItr,
                    [](const Surfel& a, const Surfel& b) {
                        return a.position.y < b.position.y;
                    });
                splitAxis = SplitAxis::kY;
            }
            else {
                std::sort(beginItr, endItr,
                    [](const Surfel& a, const Surfel& b) {
                        return a.position.z < b.position.z;
                    });
                splitAxis = SplitAxis::kZ;
            }
            // preventing the case where two surfels whose position overlaps with each other won't be split
            i32 mid = start + 1;
            if ((end - start) == 2) {
                // mid = start + 1;
            }
            else {
                switch (splitAxis) {
                case SplitAxis::kX:
                    {
                        f32 split = aabb.pmin.x + extentX * .5f;
                        /* note:
                        * i starts at 1 to make sure that there is always a split happening or else
                        * will run into issues when duplicated surfels exist
                        */
                        for (i32 i = 1; i < (end - start); ++i) {
                            if (surfels[start + i].position.x > split) {
                                mid = start + i;
                                break;
                            }
                        }
                    }
                    break;
                case SplitAxis::kY:
                    {
                        f32 split = aabb.pmin.y + extentY * .5f;
                        for (i32 i = 1; i < (end - start); ++i) {
                            if (surfels[start + i].position.y > split) {
                                mid = start + i;
                                break;
                            }
                        }
                    }
                    break;
                case SplitAxis::kZ:
                    {
                        f32 split = aabb.pmin.z + extentZ * .5f;
                        for (i32 i = 1; i < (end - start); ++i) {
                            if (surfels[start + i].position.z > split) {
                                mid = start + i;
                                break;
                            }
                        }
                    }
                    break;
                default:
                    assert(0);
                };
            }
            i32 leftChildIndex;
            i32 rightChildIndex;
            if (mid == start) {
                leftChildIndex = -1;
                assert(end > mid);
                rightChildIndex = allocNode();
            }
            else if (mid == end) {
                leftChildIndex = allocNode();
                rightChildIndex = -1;
            }
            else {
                leftChildIndex = allocNode();
                rightChildIndex = allocNode();
            }
            buildInternal(leftChildIndex, surfels, start, mid);
            buildInternal(rightChildIndex, surfels, mid, end);
            // update current node's data based on two child nodes
            nodes[nodeIndex].build(nodes, leftChildIndex, rightChildIndex);
        }
        else {
            // unreachable
            assert(0);
        }
    }

    void SurfelBSH::visualize(GfxContext* gfxc, RenderTarget* dstRenderTarget) {
        enum class VisMode : u32 {
            kBoundingSphere = 0,
            kAlbedo,
            kRadiance,
            kCount
        };

        nodeInstanceBuffer.data.array.clear();
        levelFirstTraversal(activeVisLevel, numVisLevels, [this](i32 nodeIndex) {
            glm::mat4 transform(1.f);
            // todo: found a bug here, because some node's normal for some reason becomes 'nan'
            glm::mat4 tangentFrame = calcTangentFrame(glm::normalize(nodes[nodeIndex].normal));
            transform = glm::translate(transform, nodes[nodeIndex].center);
            transform = transform * tangentFrame;
            transform = glm::scale(transform, glm::vec3(nodes[nodeIndex].radius));
            InstanceDesc instanceDesc = { };
            instanceDesc.transform = transform;
            instanceDesc.albedo = glm::vec4(nodes[nodeIndex].albedo, 1.f);
            instanceDesc.debugColor = glm::vec4(1.f, 0.f, 0.f, 1.f);
            nodeInstanceBuffer.addElement(instanceDesc);
        });
        nodeInstanceBuffer.upload();
        nodeInstanceBuffer.bind(72);
        // visualize bounding spheres
        if (bVisualizeBoundingSpheres)
        {
            Mesh* boundingSphere = AssetManager::getAsset<Mesh>("BoundingSphere");
            auto va = boundingSphere->getSubmesh(0)->getVertexArray();
            auto shader = ShaderManager::createShader({ 
                ShaderSource::Type::kVsPs, "VisualizeSurfelTreeShader", 
                SHADER_SOURCE_PATH "visualize_surfel_tree_v.glsl", 
                SHADER_SOURCE_PATH "visualize_surfel_tree_p.glsl"
            });
            gfxc->setShader(shader);
            shader->setUniform("visMode", (u32)VisMode::kBoundingSphere);
            gfxc->setRenderTarget(dstRenderTarget);
            gfxc->setViewport({ 0, 0, dstRenderTarget->width, dstRenderTarget->height });
            gfxc->setVertexArray(va);
            if (va->hasIndexBuffer()) {
                glDrawElementsInstanced(GL_LINES, boundingSphere->getSubmesh(0)->numIndices(), GL_UNSIGNED_INT, 0, nodeInstanceBuffer.getNumElements());
            }
            else {
                glDrawArraysInstanced(GL_LINES, 0, boundingSphere->getSubmesh(0)->numVertices(), nodeInstanceBuffer.getNumElements());
            }
        }
        // visualize nodes
        if (bVisualizeNodes)
        {
            Mesh* disc = AssetManager::getAsset<Mesh>("Disk");
            auto va = disc->getSubmesh(0)->getVertexArray();
            auto shader = ShaderManager::createShader({ 
                ShaderSource::Type::kVsPs, "VisualizeSurfelTreeShader", 
                SHADER_SOURCE_PATH "visualize_surfel_tree_v.glsl", 
                SHADER_SOURCE_PATH "visualize_surfel_tree_p.glsl"
            });
            gfxc->setShader(shader);
            shader->setUniform("visMode", (u32)VisMode::kAlbedo);
            gfxc->setRenderTarget(dstRenderTarget);
            gfxc->setViewport({ 0, 0, dstRenderTarget->width, dstRenderTarget->height });
            gfxc->setVertexArray(va);
            if (va->hasIndexBuffer()) {
                glDrawElementsInstanced(GL_TRIANGLES, disc->getSubmesh(0)->numIndices(), GL_UNSIGNED_INT, 0, nodeInstanceBuffer.getNumElements());
            }
            else {
                glDrawArraysInstanced(GL_TRIANGLES, 0, disc->getSubmesh(0)->numVertices(), nodeInstanceBuffer.getNumElements());
            }
        }
    }

#if 0
    void CompleteSurfelBSH::build(const std::vector<Surfel>& inSurfels) {
        if (root) {
            surfels.clear();
            // todo: release all previously created nodes
        }
        assert(!root);
        surfels = inSurfels;
        assert(((surfels.size() - 1) & (surfels.size())) == 0);
        // make sure that number of surfels is always power of 2 or else we won't be able to make a complete binary tree
        u32 numNodes = 0;
        if (surfels.size() > 0) {
            numNodes = surfels.size() * 2 - 1;
        }
        nodes.resize(numNodes);
        // create root
        root = !nodes.empty() ? &nodes[0] : nullptr;
        buildInternal(0, surfels, 0, surfels.size());
    }

    i32 CompleteSurfelBSH::getNumLevels() {
        return log2(nodes.size() + 1) - 1;
    }

    void CompleteSurfelBSH::visualize(GfxContext* gfxc, RenderTarget* dstRenderTarget) {
        enum class VisMode : u32 {
            kBoundingSphere = 0,
            kAlbedo,
            kRadiance,
            kCount
        };

        u32 startNodeIndex = exp2(activeVisLevel) - 1;
        u32 endLevel = glm::min(activeVisLevel + numVisLevels - 1, getNumLevels());
        u32 endNodeIndex = exp2(endLevel + 1) - 1 - 1;
        u32 numVisNodes = endNodeIndex - startNodeIndex + 1;

        nodeInstanceBuffer.data.array.resize(numVisNodes);
        for (i32 i = 0; i < numVisNodes; ++i) {
            u32 nodeIndex = startNodeIndex + i;
            glm::mat4 transform(1.f);
            // todo: found a bug here, because some node's normal for some reason becomes 'nan'
            glm::mat4 tangentFrame = calcTangentFrame(glm::normalize(nodes[nodeIndex].normal));
            transform = glm::translate(transform, nodes[nodeIndex].center);
            transform = transform * tangentFrame;
            transform = glm::scale(transform, glm::vec3(nodes[nodeIndex].radius));
            nodeInstanceBuffer[i].transform = transform;
            nodeInstanceBuffer[i].albedo = glm::vec4(nodes[nodeIndex].albedo, 1.f);
            nodeInstanceBuffer[i].debugColor = glm::vec4(1.f, 0.f, 0.f, 1.f);
        };
        nodeInstanceBuffer.upload();
        nodeInstanceBuffer.bind(72);
        // visualize bounding spheres
        if (bVisualizeBoundingSpheres)
        {
            Mesh* boundingSphere = AssetManager::getAsset<Mesh>("BoundingSphere");
            auto va = boundingSphere->getSubmesh(0)->getVertexArray();
            auto shader = ShaderManager::createShader({ 
                ShaderSource::Type::kVsPs, "VisualizeSurfelTreeShader", 
                SHADER_SOURCE_PATH "visualize_surfel_tree_v.glsl", 
                SHADER_SOURCE_PATH "visualize_surfel_tree_p.glsl"
            });
            gfxc->setShader(shader);
            shader->setUniform("visMode", (u32)VisMode::kBoundingSphere);
            gfxc->setRenderTarget(dstRenderTarget);
            gfxc->setViewport({ 0, 0, dstRenderTarget->width, dstRenderTarget->height });
            gfxc->setVertexArray(va);
            if (va->hasIndexBuffer()) {
                glDrawElementsInstanced(GL_LINES, boundingSphere->getSubmesh(0)->numIndices(), GL_UNSIGNED_INT, 0, nodeInstanceBuffer.getNumElements());
            }
            else {
                glDrawArraysInstanced(GL_LINES, 0, boundingSphere->getSubmesh(0)->numVertices(), nodeInstanceBuffer.getNumElements());
            }
        }
        // visualize nodes
        if (bVisualizeNodes)
        {
            Mesh* disc = AssetManager::getAsset<Mesh>("Disk");
            auto va = disc->getSubmesh(0)->getVertexArray();
            auto shader = ShaderManager::createShader({ 
                ShaderSource::Type::kVsPs, "VisualizeSurfelTreeShader", 
                SHADER_SOURCE_PATH "visualize_surfel_tree_v.glsl", 
                SHADER_SOURCE_PATH "visualize_surfel_tree_p.glsl"
            });
            gfxc->setShader(shader);
            shader->setUniform("visMode", (u32)VisMode::kAlbedo);
            gfxc->setRenderTarget(dstRenderTarget);
            gfxc->setViewport({ 0, 0, dstRenderTarget->width, dstRenderTarget->height });
            gfxc->setVertexArray(va);
            if (va->hasIndexBuffer()) {
                glDrawElementsInstanced(GL_TRIANGLES, disc->getSubmesh(0)->numIndices(), GL_UNSIGNED_INT, 0, nodeInstanceBuffer.getNumElements());
            }
            else {
                glDrawArraysInstanced(GL_TRIANGLES, 0, disc->getSubmesh(0)->numVertices(), nodeInstanceBuffer.getNumElements());
            }
        }
    }

    void CompleteSurfelBSH::debugVisualize(GfxContext* gfxc, RenderTarget* dstRenderTarget) {
        u32 startNodeIndex = exp2(activeVisLevel) - 1;
        u32 endLevel = glm::min(activeVisLevel + numVisLevels - 1, getNumLevels());
        u32 endNodeIndex = exp2(endLevel + 1) - 1 - 1;
        u32 numVisNodes = endNodeIndex - startNodeIndex + 1;
    }

    i32 leftChildOf(i32 parent) {
        return parent * 2 + 1;
    }

    i32 rightChildOf(i32 parent) {
        return parent * 2 + 2;
    }

    void CompleteSurfelBSH::buildInternal(i32 nodeIndex, std::vector<Surfel>& surfels, u32 start, u32 end) {
        // leaf node
        if ((end - start) == 1u) { 
            nodes[nodeIndex].build(surfels, start);
        }
        else if ((end - start) > 1u) {
            // determine dominant axis
            // todo: found a bug here, the centroid
            BoundingBox3D aabb;
            for (u32 i = 0; i < (end - start); ++i) {
                aabb.bound(surfels[start + i].position);
            }
            f32 extentX = abs(aabb.pmax.x - aabb.pmin.x);
            f32 extentY = abs(aabb.pmax.y - aabb.pmin.y);
            f32 extentZ = abs(aabb.pmax.z - aabb.pmin.z);
            // sort in dominant axis
            const auto& beginItr = surfels.begin() + start;
            const auto& endItr = surfels.begin() + end;
            if (extentX >= glm::max(extentY, extentZ)) {
                std::sort(beginItr, endItr,
                    [](const Surfel& a, const Surfel& b) {
                        return a.position.x < b.position.x;
                    });
            }
            else if (extentY >= glm::max(extentX, extentZ)) {
                std::sort(beginItr, endItr,
                    [](const Surfel& a, const Surfel& b) {
                        return a.position.y < b.position.y;
                    });
            }
            else {
                std::sort(beginItr, endItr,
                    [](const Surfel& a, const Surfel& b) {
                        return a.position.z < b.position.z;
                    });
            }
            // recursively split the surfels 
            u32 mid = (start + end) / 2;
            u32 leftChildIndex = leftChildOf(nodeIndex);
            u32 rightChildIndex = rightChildOf(nodeIndex);
            buildInternal(leftChildIndex, surfels, start, mid);
            buildInternal(rightChildIndex, surfels, mid, end);

            // update current node's data based on two child nodes
            nodes[nodeIndex].build(nodes, leftChildIndex, rightChildIndex);
        }
        else {
            cyanError("Num of surfels is not power of 2");
            assert(0);
        }
    }
#endif
};