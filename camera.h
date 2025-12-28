#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    glm::vec3 position{0.0f, 0.0f, 5.0f};
    float yaw = -90.0f;   // facing -Z
    float pitch = 0.0f;
    float speed = 5.0f;
    float sensitivity = 0.1f;
    float fov = 60.0f;

    void processKeyboard(const uint8_t* keys, float dt);
    void processMouse(int dx, int dy);

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspect) const;
    glm::vec3 getForward() const;
    glm::vec3 getRight() const;

private:
    void updateVectors();
    glm::vec3 front{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};
};
