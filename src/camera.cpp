#include <iostream>
#include "glfw3.h"
#include "gtc/matrix_transform.hpp"
#include "camera.h"

// Default camera settings
Camera::Camera() {
    position = glm::vec3(1.f, 2.f, 5.f);
    worldUp = glm::vec3(0.f, 1.f, 0.f);
    forward = glm::vec3(0.f, 0.f, -1.f);
    fov = 50.f;
    near = 0.1f;
    far = 100.f;
    yaw = glm::radians(180.f);
    pitch = 0.f;
}

void Camera::processKeyboard(int key, float deltaTime) {
    switch (key) {
        case GLFW_KEY_W: {
            position += forward * deltaTime;
            break;
        }
        case GLFW_KEY_S: {
            position += -forward * deltaTime;
            break;
        }
        case GLFW_KEY_A: {
            position += -right * deltaTime;
            break;
        }
        case GLFW_KEY_D: {
            position += right * deltaTime;
            break;
        }
    }
}

void Camera::processMouse(double deltaX, double deltaY, float deltaTime) {
    yaw += glm::radians(-deltaX * .2f);
    pitch += glm::radians(-deltaY * .2f);
    forward.x = std::cos(pitch) * std::sin(yaw);
    forward.y = std::sin(pitch);
    forward.z = std::cos(pitch) * std::cos(yaw);
}

void Camera::updateView() {
    glm::mat4 view(1.f);
    right = glm::cross(forward, worldUp);
    glm::vec3 up = glm::cross(right, forward);
    
    switch (eMode)
    {
        case ControlMode::free: {
            view *= glm::lookAt(position, position + forward, worldUp);
            break;
        }
        
        case ControlMode::orbit: {
            glm::mat4x4 rotMat(1.f);
            float r = glm::length(position);
            view = glm::translate(view, glm::vec3(-forward * r));
            rotMat[0] = glm::vec4(right, 0.f);
            rotMat[1] = glm::vec4(up, 0.f);
            rotMat[2] = glm::vec4(-forward, 0.f);
            rotMat[3] = glm::vec4(0.f, 0.f, 0.f, 1.f);
            rotMat = glm::transpose(rotMat);
            view *= rotMat;
            break;
        }
    }
    this->view = view;
}
