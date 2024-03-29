#pragma once

#include "glm.hpp"
#include "gtx/quaternion.hpp"

struct Rotation
{
    float angle;
    glm::vec3 axis;
};

struct Transform
{
    glm::vec3 m_translate;
    glm::quat m_qRot;
    glm::vec3 m_scale;

    Transform(glm::vec3 translation=glm::vec3(0.f), glm::quat rotation=glm::quat(1.f, glm::vec3(0.f, 0.f, 0.f)), glm::vec3 scale=glm::vec3(1.f))
    {
        m_translate = translation;
        m_qRot = rotation;
        m_scale = scale;
    }

    Transform(const glm::mat4& mat)
    {
        fromMatrix(mat);
    }
    
    glm::mat4 toMatrix() const
    {
        glm::mat4 mat(1.f);
        mat = glm::translate(mat, m_translate);
        glm::mat4 rotation = glm::toMat4(m_qRot);
        mat = mat * rotation;
        mat = glm::scale(mat, m_scale);
        return mat;
    }

    void fromMatrix(glm::mat4 mat) {
        m_translate = glm::vec3(mat[3].x, mat[3].y, mat[3].z); 
        m_scale = glm::vec3(glm::length(glm::vec3(mat[0].x, mat[0].y, mat[0].z)), 
                            glm::length(glm::vec3(mat[1].x, mat[1].y, mat[1].z)),
                            glm::length(glm::vec3(mat[2].x, mat[2].y, mat[2].z)));
        // clear translation
        mat[3] = glm::vec4(0.f, 0.f, 0.f, 1.f);
        // clear scale
        mat[0] *= 1.f / m_scale.x;
        mat[1] *= 1.f / m_scale.y;
        mat[2] *= 1.f / m_scale.z;
        m_qRot = glm::toQuat(mat);
    }

    void operator=(Transform rhs)
    {
        m_translate = rhs.m_translate;
        m_qRot = rhs.m_qRot;
        m_scale = rhs.m_scale;
    }

    void rotate()
    {

    }

    void translate()
    {

    }

    void scale()
    {

    }
};