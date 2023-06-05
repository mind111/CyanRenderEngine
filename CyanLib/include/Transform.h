#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Cyan
{
    struct Rotation
    {
        float angle;
        glm::vec3 axis;
    };

    struct Transform
    {
        Transform()
        {
            translation = glm::vec3(0.f);
            rotation = glm::quat(1.f, glm::vec3(0.f));
            scale = glm::vec3(1.f);
        }

        Transform(const glm::vec3& t, const glm::quat& r, const glm::vec3& s)
        {
            translation = t;
            rotation = r;
            scale = s;
        }

        Transform(const glm::mat4& mat)
        {
            fromMatrix(mat);
        }
        
        glm::mat4 toMatrix() const
        {
            glm::mat4 mat(1.f);
            mat = glm::translate(mat, translation);
            glm::mat4 rotationMatrix = glm::toMat4(rotation);
            mat = mat * rotationMatrix;
            mat = glm::scale(mat, scale);
            return mat;
        }

        void fromMatrix(glm::mat4 mat) {
            translation = glm::vec3(mat[3].x, mat[3].y, mat[3].z); 
            scale = glm::vec3(glm::length(glm::vec3(mat[0].x, mat[0].y, mat[0].z)), 
                                glm::length(glm::vec3(mat[1].x, mat[1].y, mat[1].z)),
                                glm::length(glm::vec3(mat[2].x, mat[2].y, mat[2].z)));
            // clear translation
            mat[3] = glm::vec4(0.f, 0.f, 0.f, 1.f);
            // clear scale
            mat[0] *= 1.f / scale.x;
            mat[1] *= 1.f / scale.y;
            mat[2] *= 1.f / scale.z;
            rotation = glm::toQuat(mat);
        }

        void operator=(Transform rhs)
        {
            translation = rhs.translation;
            rotation = rhs.rotation;
            scale = rhs.scale;
        }

        glm::vec3 translation;
        glm::quat rotation;
        glm::vec3 scale;
    };
}