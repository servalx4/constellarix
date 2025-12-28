#pragma once
#include "graph.h"
#include "camera.h"
#include <GL/glew.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <unordered_map>

class Renderer {
public:
    bool init();
    void shutdown();

    void begin(const Camera& camera, int screenW, int screenH);
    void renderGraph(const Graph& graph, int selectedNode, const Camera& camera, int screenW, int screenH, float dt,
                     bool showNodes = true, bool showLinks = true, bool showLabels = true, bool domainColors = false);
    void renderCrosshair(int screenW, int screenH);
    void renderText2D(const std::string& text, float x, float y, glm::vec3 color);
    void renderAddressBar(const std::string& text, int screenW, int screenH, bool active);
    void renderVisibilityMenu(int screenW, int screenH, int selection, bool showNodes, bool showLinks, bool showLabels, bool domainColors, bool showStats);
    void renderStats(int screenW, int screenH, int nodeCount, int edgeCount, int pendingCount);

private:
    GLuint nodeShader = 0, lineShader = 0, textShader = 0, uiShader = 0, roundedShader = 0;
    GLuint billboardVAO = 0, billboardVBO = 0, instanceVBO = 0;
    GLuint batchedLineVAO = 0, batchedLineVBO = 0;
    GLuint textVAO = 0, textVBO = 0;
    GLuint quadVAO = 0, quadVBO = 0;
    GLuint starTexture = 0;

    std::vector<float> lineVertices;
    std::vector<float> nodeInstances;

    TTF_Font* font = nullptr;
    struct CachedText { GLuint tex; int w, h; };

    // Per-label state for smooth transitions
    struct LabelState {
        float x = 0, y = 0;           // Current screen position
        float targetX = 0, targetY = 0; // Target position
        float opacity = 0;            // Current opacity (0 = hidden, 1 = visible)
        bool visible = false;         // Should be visible this frame
    };
    std::unordered_map<int, LabelState> labelStates;

    glm::mat4 view, proj;
    int screenWidth = 0, screenHeight = 0;

    GLuint compileShader(const char* vert, const char* frag);
    bool loadStarTexture();
    bool loadFont();
    void initBillboard();
    void initLineMesh();
    void initTextQuad();
    void initQuad();
    CachedText getTextTexture(const std::string& text);
    glm::vec3 worldToScreen(const glm::vec3& worldPos);
};
