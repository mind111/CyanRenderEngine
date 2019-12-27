#pragma once
#include "glm.hpp"

enum class ControlMode {
    free,
    orbit
};

class Camera {
public:
    glm::vec3 position, target;
    glm::vec3 forward, right;
    glm::vec3 worldUp;
    glm::mat4 view, projection;
    float fov, near, far;

    // Rotation in degree
    float yaw, pitch;

    // Camera control mode
    ControlMode eMode;

    Camera();

    void processKeyboard(int key, float deltaTime); 
    void processMouse(double deltaX, double deltaY, float deltaTime);
    void updateView();
};