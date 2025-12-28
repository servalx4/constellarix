#pragma once
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <string>

class Window {
public:
    bool init(int width, int height, const std::string& title);
    void shutdown();
    void swap();
    bool shouldClose() const { return closed; }
    void close() { closed = true; }

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    SDL_Window* getSDLWindow() { return window; }
    void toggleFullscreen();
    void updateSize();

private:
    SDL_Window* window = nullptr;
    SDL_GLContext glContext = nullptr;
    int width = 0, height = 0;
    bool closed = false;
    bool fullscreen = false;
};
