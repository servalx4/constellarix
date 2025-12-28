#include "ui.h"

void UI::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        if (!addressBarActive && !menuOpen) {
            if (event.key.keysym.sym == SDLK_RETURN) {
                addressBarActive = true;
                addressBarText.clear();
                SDL_SetRelativeMouseMode(SDL_FALSE);
            } else if (event.key.keysym.sym == SDLK_r) {
                menuOpen = true;
                menuSelection = 0;
            }
        } else if (menuOpen) {
            if (event.key.keysym.sym == SDLK_r || event.key.keysym.sym == SDLK_ESCAPE) {
                menuOpen = false;
            } else if (event.key.keysym.sym == SDLK_UP) {
                menuSelection = (menuSelection + 4) % 5; // Wrap around
            } else if (event.key.keysym.sym == SDLK_DOWN) {
                menuSelection = (menuSelection + 1) % 5;
            } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_SPACE) {
                if (menuSelection == 0) showNodes = !showNodes;
                else if (menuSelection == 1) showLinks = !showLinks;
                else if (menuSelection == 2) showLabels = !showLabels;
                else if (menuSelection == 3) domainColors = !domainColors;
                else if (menuSelection == 4) showStats = !showStats;
            } else if (event.key.keysym.sym == SDLK_1) {
                showNodes = !showNodes;
            } else if (event.key.keysym.sym == SDLK_2) {
                showLinks = !showLinks;
            } else if (event.key.keysym.sym == SDLK_3) {
                showLabels = !showLabels;
            } else if (event.key.keysym.sym == SDLK_4) {
                domainColors = !domainColors;
            } else if (event.key.keysym.sym == SDLK_5) {
                showStats = !showStats;
            }
        } else if (addressBarActive) {
            if (event.key.keysym.sym == SDLK_RETURN) {
                if (!addressBarText.empty()) {
                    submitted = true;
                }
                addressBarActive = false;
                SDL_SetRelativeMouseMode(SDL_TRUE);
            } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                addressBarActive = false;
                addressBarText.clear();
                SDL_SetRelativeMouseMode(SDL_TRUE);
            } else if (event.key.keysym.sym == SDLK_BACKSPACE) {
                if (!addressBarText.empty()) {
                    addressBarText.pop_back();
                }
            }
        }
    } else if (event.type == SDL_TEXTINPUT && addressBarActive) {
        addressBarText += event.text.text;
    }
}

std::string UI::consumeSubmittedUrl() {
    submitted = false;
    std::string url = addressBarText;
    addressBarText.clear();

    // Add https:// if no protocol
    if (url.find("://") == std::string::npos) {
        url = "https://" + url;
    }
    return url;
}
