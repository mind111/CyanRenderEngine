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

    // construct defualt transform
    Transform()
    {
        m_translate = glm::vec3(0.f);    
        // unit quaternion
        m_qRot = glm::quat(1.f, glm::vec3(0.f));
        m_scale = glm::vec3(1.f);
    }

    Transform(glm::vec3 translation, glm::quat rotation, glm::vec3 scale)
    {
        m_translate = translation;    
        m_qRot = rotation;
        m_scale = scale;
    }
    
    glm::mat4 toMatrix()
    {
        glm::mat4 mat(1.f);
        mat = glm::translate(mat, m_translate);
        glm::mat4 rotation = glm::toMat4(m_qRot);
        mat = mat * rotation;
        mat = glm::scale(mat, m_scale);
        return mat;
    }

    void operator=(Transform rhs)
    {
        m_translate = rhs.m_translate;
        m_qRot = rhs.m_qRot;
        m_scale = rhs.m_scale;
    }
};