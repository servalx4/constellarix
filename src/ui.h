#pragma once
#include <string>
#include <SDL2/SDL.h>

class UI {
public:
    bool addressBarActive = false;
    std::string addressBarText;

    // Visibility menu
    bool menuOpen = false;
    int menuSelection = 0;
    bool showNodes = true;
    bool showLinks = true;
    bool showLabels = true;
    bool domainColors = false;
    bool showStats = false;

    void handleEvent(const SDL_Event& event);
    bool hasSubmittedUrl() const { return submitted; }
    std::string consumeSubmittedUrl();

private:
    bool submitted = false;
};
