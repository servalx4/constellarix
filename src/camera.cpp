#include "camera.h"
#include <SDL2/SDL.h>
#include <algorithm>

void Camera::updateVectors() {
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(front);

    right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
    up = glm::normalize(glm::cross(right, front));
}

void Camera::processKeyboard(const uint8_t* keys, float dt) {
    float velocity = speed * dt;
    if (keys[SDL_SCANCODE_LSHIFT]) velocity *= 2.5f;

    if (keys[SDL_SCANCODE_W]) position += front * velocity;
    if (keys[SDL_SCANCODE_S]) position -= front * velocity;
    if (keys[SDL_SCANCODE_A]) position -= right * velocity;
    if (keys[SDL_SCANCODE_D]) position += right * velocity;
    if (keys[SDL_SCANCODE_SPACE]) position.y += velocity;
    if (keys[SDL_SCANCODE_LCTRL]) position.y -= velocity;
}

void Camera::processMouse(int dx, int dy) {
    yaw += dx * sensitivity;
    pitch -= dy * sensitivity;
    pitch = std::clamp(pitch, -89.0f, 89.0f);
    updateVectors();
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::getProjectionMatrix(float aspect) const {
    return glm::perspective(glm::radians(fov), aspect, 0.1f, 1000.0f);
}

glm::vec3 Camera::getForward() const { return front; }
glm::vec3 Camera::getRight() const { return right; }
