#include "renderer.h"
#include "star_png.h"
#include "font_ttf.h"
#include <SDL2/SDL_image.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cmath>
#include <algorithm>

// Billboard vertex shader with instancing
static const char* nodeVertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aWorldPos;
layout(location = 3) in float aSize;
layout(location = 4) in vec3 aColor;
out vec2 vUV;
out vec3 vColor;
uniform mat4 uView;
uniform mat4 uProj;
void main() {
    vec3 camRight = vec3(uView[0][0], uView[1][0], uView[2][0]);
    vec3 camUp = vec3(uView[0][1], uView[1][1], uView[2][1]);
    vec3 worldPos = aWorldPos + camRight * aPos.x * aSize + camUp * aPos.y * aSize;
    gl_Position = uProj * uView * vec4(worldPos, 1.0);
    vUV = aUV;
    vColor = aColor;
}
)";

static const char* nodeFragSrc = R"(
#version 330 core
in vec2 vUV;
in vec3 vColor;
out vec4 fragColor;
uniform sampler2D uTexture;
void main() {
    vec4 texColor = texture(uTexture, vUV);
    if (texColor.a < 0.1) discard;
    fragColor = vec4(vColor * texColor.rgb, texColor.a);
}
)";

static const char* lineVertSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
out vec4 vColor;
uniform mat4 uVP;
void main() {
    gl_Position = uVP * vec4(aPos, 1.0);
    vColor = aColor;
}
)";

static const char* lineFragSrc = R"(
#version 330 core
in vec4 vColor;
out vec4 fragColor;
void main() {
    fragColor = vColor;
}
)";

static const char* uiVertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform mat4 uProj;
uniform vec2 uOffset;
uniform vec2 uScale;
void main() {
    gl_Position = uProj * vec4(aPos * uScale + uOffset, 0.0, 1.0);
}
)";

static const char* uiFragSrc = R"(
#version 330 core
out vec4 fragColor;
uniform vec3 uColor;
void main() {
    fragColor = vec4(uColor, 1.0);
}
)";

static const char* roundedVertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vLocalPos;
uniform mat4 uProj;
uniform vec2 uOffset;
uniform vec2 uScale;
void main() {
    vLocalPos = aPos * uScale;
    gl_Position = uProj * vec4(aPos * uScale + uOffset, 0.0, 1.0);
}
)";

static const char* roundedFragSrc = R"(
#version 330 core
in vec2 vLocalPos;
out vec4 fragColor;
uniform vec3 uColor;
uniform vec2 uSize;
uniform float uRadius;
void main() {
    vec2 p = vLocalPos;
    vec2 q = abs(p - uSize * 0.5) - uSize * 0.5 + uRadius;
    float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - uRadius;
    if (d > 0.0) discard;
    fragColor = vec4(uColor, 1.0);
}
)";

static const char* textVertSrc = R"(
#version 330 core
layout(location = 0) in vec4 aVertex; // xy = pos, zw = uv
out vec2 vUV;
uniform mat4 uProj;
void main() {
    gl_Position = uProj * vec4(aVertex.xy, 0.0, 1.0);
    vUV = aVertex.zw;
}
)";

static const char* textFragSrc = R"(
#version 330 core
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uTexture;
uniform vec3 uColor;
void main() {
    vec4 tex = texture(uTexture, vUV);
    fragColor = vec4(uColor, tex.a);
}
)";

GLuint Renderer::compileShader(const char* vert, const char* frag) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vert, nullptr);
    glCompileShader(vs);

    GLint success;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(vs, 512, nullptr, log);
        std::cerr << "Vertex shader error: " << log << "\n";
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &frag, nullptr);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(fs, 512, nullptr, log);
        std::cerr << "Fragment shader error: " << log << "\n";
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

bool Renderer::loadStarTexture() {
    SDL_RWops* rw = SDL_RWFromConstMem(star_png, star_png_len);
    if (!rw) return false;

    SDL_Surface* surface = IMG_Load_RW(rw, 1);
    if (!surface) return false;

    SDL_Surface* converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);
    if (!converted) return false;

    glGenTextures(1, &starTexture);
    glBindTexture(GL_TEXTURE_2D, starTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, converted->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);

    SDL_FreeSurface(converted);
    return true;
}

bool Renderer::loadFont() {
    SDL_RWops* rw = SDL_RWFromConstMem(font_ttf, font_ttf_len);
    if (!rw) {
        std::cerr << "Failed to create RWops for font\n";
        return false;
    }

    font = TTF_OpenFontRW(rw, 1, 14); // 14pt
    if (!font) {
        std::cerr << "Failed to load font: " << TTF_GetError() << "\n";
        return false;
    }
    return true;
}

void Renderer::initBillboard() {
    float verts[] = {
        -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.0f, 1.0f,
    };

    glGenVertexArrays(1, &billboardVAO);
    glGenBuffers(1, &billboardVBO);
    glGenBuffers(1, &instanceVBO);

    glBindVertexArray(billboardVAO);

    // Static quad vertices
    glBindBuffer(GL_ARRAY_BUFFER, billboardVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Instance data: vec3 pos, float size, vec3 color = 7 floats per instance
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, 100000 * 7 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // aWorldPos (location 2)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    // aSize (location 3)
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);

    // aColor (location 4)
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribDivisor(4, 1);
}

void Renderer::initLineMesh() {
    // Batched lines: pos (3) + color (4) = 7 floats per vertex, 2 vertices per line
    glGenVertexArrays(1, &batchedLineVAO);
    glGenBuffers(1, &batchedLineVBO);
    glBindVertexArray(batchedLineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, batchedLineVBO);
    glBufferData(GL_ARRAY_BUFFER, 500000 * 14 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Color
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void Renderer::initTextQuad() {
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
}

void Renderer::initQuad() {
    float quad[] = {
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
    };
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
}

Renderer::CachedText Renderer::getTextTexture(const std::string& text) {
    if (text.empty()) return {0, 0, 0};
    if (!font) return {0, 0, 0};

    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), white);
    if (!surface) return {0, 0, 0};

    SDL_Surface* converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_FreeSurface(surface);
    if (!converted) return {0, 0, 0};

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, converted->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    CachedText result = {tex, converted->w, converted->h};
    SDL_FreeSurface(converted);
    return result;
}

glm::vec3 Renderer::worldToScreen(const glm::vec3& worldPos) {
    glm::vec4 clip = proj * view * glm::vec4(worldPos, 1.0f);
    if (clip.w <= 0) return glm::vec3(-1000, -1000, -1); // Behind camera

    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    float x = (ndc.x + 1.0f) * 0.5f * screenWidth;
    float y = (1.0f - ndc.y) * 0.5f * screenHeight;
    return glm::vec3(x, y, ndc.z);
}

bool Renderer::init() {
    if (IMG_Init(IMG_INIT_PNG) == 0) {
        std::cerr << "SDL_image init failed\n";
        return false;
    }

    if (TTF_Init() < 0) {
        std::cerr << "SDL_ttf init failed\n";
        return false;
    }

    nodeShader = compileShader(nodeVertSrc, nodeFragSrc);
    lineShader = compileShader(lineVertSrc, lineFragSrc);
    uiShader = compileShader(uiVertSrc, uiFragSrc);
    textShader = compileShader(textVertSrc, textFragSrc);
    roundedShader = compileShader(roundedVertSrc, roundedFragSrc);

    if (!loadStarTexture()) {
        std::cerr << "Warning: failed to load star texture\n";
    }

    if (!loadFont()) {
        std::cerr << "Warning: failed to load font\n";
    }

    initBillboard();
    initLineMesh();
    initTextQuad();
    initQuad();

    return true;
}

void Renderer::shutdown() {
    if (font) TTF_CloseFont(font);
    TTF_Quit();
    IMG_Quit();

    glDeleteProgram(nodeShader);
    glDeleteProgram(lineShader);
    glDeleteProgram(uiShader);
    glDeleteProgram(textShader);
    glDeleteProgram(roundedShader);
    glDeleteVertexArrays(1, &billboardVAO);
    glDeleteBuffers(1, &billboardVBO);
    glDeleteBuffers(1, &instanceVBO);
    glDeleteVertexArrays(1, &batchedLineVAO);
    glDeleteBuffers(1, &batchedLineVBO);
    glDeleteVertexArrays(1, &textVAO);
    glDeleteBuffers(1, &textVBO);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteTextures(1, &starTexture);
}

void Renderer::begin(const Camera& camera, int screenW, int screenH) {
    screenWidth = screenW;
    screenHeight = screenH;

    glClearColor(0.01f, 0.01f, 0.02f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    view = camera.getViewMatrix();
    proj = camera.getProjectionMatrix((float)screenW / screenH);
}

// Hash a domain string to a vibrant HSL color
static glm::vec3 domainToColor(const std::string& url) {
    // Extract domain from URL
    std::string domain = url;
    size_t protoEnd = domain.find("://");
    if (protoEnd != std::string::npos) {
        domain = domain.substr(protoEnd + 3);
    }
    // Remove path
    size_t pathStart = domain.find('/');
    if (pathStart != std::string::npos) {
        domain = domain.substr(0, pathStart);
    }
    // Remove port
    size_t portStart = domain.find(':');
    if (portStart != std::string::npos) {
        domain = domain.substr(0, portStart);
    }
    // Remove userinfo (user:pass@)
    size_t atSign = domain.find('@');
    if (atSign != std::string::npos) {
        domain = domain.substr(atSign + 1);
    }
    // Lowercase for consistency
    for (char& c : domain) {
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    }

    // Extract base domain (e.g., "news.google.com" -> "google.com")
    std::vector<std::string> parts;
    size_t start = 0, end;
    while ((end = domain.find('.', start)) != std::string::npos) {
        parts.push_back(domain.substr(start, end - start));
        start = end + 1;
    }
    parts.push_back(domain.substr(start));

    // Handle two-part TLDs like co.uk, com.au, org.uk, etc.
    if (parts.size() >= 3) {
        const std::string& secondLast = parts[parts.size() - 2];
        if (secondLast == "co" || secondLast == "com" || secondLast == "org" ||
            secondLast == "net" || secondLast == "gov" || secondLast == "edu" ||
            secondLast == "ac" || secondLast == "or") {
            // Take last 3 parts (e.g., bbc.co.uk)
            domain = parts[parts.size() - 3] + "." + parts[parts.size() - 2] + "." + parts[parts.size() - 1];
        } else {
            // Take last 2 parts (e.g., google.com)
            domain = parts[parts.size() - 2] + "." + parts[parts.size() - 1];
        }
    } else if (parts.size() == 2) {
        domain = parts[0] + "." + parts[1];
    }
    // else: single part domain (localhost, etc.) - use as-is

    // Simple hash
    unsigned int hash = 0;
    for (char c : domain) {
        hash = hash * 31 + c;
    }

    // Convert to HSL then RGB (high saturation, medium lightness for vibrancy)
    float hue = (hash % 360) / 360.0f;
    float sat = 0.7f;
    float lit = 0.6f;

    // HSL to RGB conversion
    auto hue2rgb = [](float p, float q, float t) {
        if (t < 0) t += 1;
        if (t > 1) t -= 1;
        if (t < 1.0f/6) return p + (q - p) * 6 * t;
        if (t < 1.0f/2) return q;
        if (t < 2.0f/3) return p + (q - p) * (2.0f/3 - t) * 6;
        return p;
    };

    float q = lit < 0.5f ? lit * (1 + sat) : lit + sat - lit * sat;
    float p = 2 * lit - q;
    float r = hue2rgb(p, q, hue + 1.0f/3);
    float g = hue2rgb(p, q, hue);
    float b = hue2rgb(p, q, hue - 1.0f/3);

    return glm::vec3(r, g, b);
}

void Renderer::renderGraph(const Graph& graph, int selectedNode, const Camera& camera, int screenW, int screenH, float dt,
                           bool showNodes, bool showLinks, bool showLabels, bool domainColors) {
    // Build and render batched line data
    lineVertices.clear();
    if (showLinks) {
        lineVertices.reserve(graph.edges.size() * 14); // 7 floats per vertex, 2 vertices per line

        for (const auto& edge : graph.edges) {
            if (edge.from < 0 || edge.from >= (int)graph.nodes.size()) continue;
            if (edge.to < 0 || edge.to >= (int)graph.nodes.size()) continue;
            const auto& nodeA = graph.nodes[edge.from];
            const auto& nodeB = graph.nodes[edge.to];

            glm::vec3 startPos = nodeA.position;
            glm::vec3 endPos = glm::mix(nodeA.position, nodeB.position, edge.fadeIn);

            glm::vec3 edgeMid = (nodeA.position + nodeB.position) * 0.5f;
            float distFromCam = glm::length(edgeMid - camera.position);
            float combinedSize = (nodeA.size + nodeB.size) * 0.5f;

            float alpha = 0.9f / (1.0f + distFromCam * distFromCam * 0.01f);
            alpha = std::max(0.05f, std::min(alpha, 0.9f));
            alpha *= (0.3f + combinedSize * 0.4f);
            alpha *= edge.fadeIn;

            float brightness = 0.15f + combinedSize * 0.25f;

            // Start vertex
            lineVertices.push_back(startPos.x);
            lineVertices.push_back(startPos.y);
            lineVertices.push_back(startPos.z);
            lineVertices.push_back(brightness);
            lineVertices.push_back(brightness);
            lineVertices.push_back(brightness + 0.15f);
            lineVertices.push_back(alpha);

            // End vertex
            lineVertices.push_back(endPos.x);
            lineVertices.push_back(endPos.y);
            lineVertices.push_back(endPos.z);
            lineVertices.push_back(brightness);
            lineVertices.push_back(brightness);
            lineVertices.push_back(brightness + 0.15f);
            lineVertices.push_back(alpha);
        }

        // Render all edges in one draw call
        glDisable(GL_DEPTH_TEST);
        glUseProgram(lineShader);
        glUniformMatrix4fv(glGetUniformLocation(lineShader, "uVP"), 1, GL_FALSE, glm::value_ptr(proj * view));

        glBindVertexArray(batchedLineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, batchedLineVBO);
        if (!lineVertices.empty()) {
            glBufferSubData(GL_ARRAY_BUFFER, 0, lineVertices.size() * sizeof(float), lineVertices.data());
        }

        glEnable(GL_LINE_SMOOTH);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, lineVertices.size() / 7);
        glLineWidth(1.0f);
        glEnable(GL_DEPTH_TEST);
    }

    // Build and render node instance data
    if (showNodes) {
        nodeInstances.clear();
        nodeInstances.reserve(graph.nodes.size() * 7);

        for (size_t i = 0; i < graph.nodes.size(); i++) {
            const auto& node = graph.nodes[i];
            if (node.fadeIn < 0.01f) continue;

            float visualSize = node.size * 0.15f * node.fadeIn;

            glm::vec3 color;
            if (domainColors && node.status == NodeStatus::Success) {
                color = domainToColor(node.url);
            } else {
                switch (node.status) {
                    case NodeStatus::Pending: color = glm::vec3(0.4f, 0.6f, 1.0f); break;
                    case NodeStatus::Success: color = glm::vec3(1.0f, 1.0f, 1.0f); break;
                    case NodeStatus::Error:   color = glm::vec3(1.0f, 0.3f, 0.3f); break;
                }
            }

            if ((int)i == selectedNode) {
                color = glm::vec3(1.0f, 1.0f, 0.4f);
            }
            color *= node.fadeIn;

            nodeInstances.push_back(node.position.x);
            nodeInstances.push_back(node.position.y);
            nodeInstances.push_back(node.position.z);
            nodeInstances.push_back(visualSize);
            nodeInstances.push_back(color.r);
            nodeInstances.push_back(color.g);
            nodeInstances.push_back(color.b);
        }

        // Render all nodes in one instanced draw call
        glUseProgram(nodeShader);
        glUniformMatrix4fv(glGetUniformLocation(nodeShader, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(nodeShader, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, starTexture);
        glUniform1i(glGetUniformLocation(nodeShader, "uTexture"), 0);

        glBindVertexArray(billboardVAO);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        if (!nodeInstances.empty()) {
            glBufferSubData(GL_ARRAY_BUFFER, 0, nodeInstances.size() * sizeof(float), nodeInstances.data());
        }

        glDepthMask(GL_FALSE);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, nodeInstances.size() / 7);
        glDepthMask(GL_TRUE);
    }

    // Render pin indicators (blue circles around pinned nodes)
    {
        std::vector<float> pinCircleVerts;
        const int segments = 24;

        for (size_t i = 0; i < graph.nodes.size(); i++) {
            const auto& node = graph.nodes[i];
            if (!node.pinned || node.fadeIn < 0.01f) continue;

            glm::vec3 screenPos = worldToScreen(node.position);
            if (screenPos.z < -1 || screenPos.z > 1) continue;

            float radius = 12.0f + node.size * 8.0f;
            float alpha = node.fadeIn * 0.8f;

            for (int s = 0; s < segments; s++) {
                float a1 = (float)s / segments * 6.28318f;
                float a2 = (float)(s + 1) / segments * 6.28318f;

                float x1 = screenPos.x + std::cos(a1) * radius;
                float y1 = screenPos.y + std::sin(a1) * radius;
                float x2 = screenPos.x + std::cos(a2) * radius;
                float y2 = screenPos.y + std::sin(a2) * radius;

                // Line segment (2D, z=0)
                pinCircleVerts.push_back(x1);
                pinCircleVerts.push_back(y1);
                pinCircleVerts.push_back(0.0f);
                pinCircleVerts.push_back(0.3f); // R
                pinCircleVerts.push_back(0.5f); // G
                pinCircleVerts.push_back(1.0f); // B
                pinCircleVerts.push_back(alpha);

                pinCircleVerts.push_back(x2);
                pinCircleVerts.push_back(y2);
                pinCircleVerts.push_back(0.0f);
                pinCircleVerts.push_back(0.3f);
                pinCircleVerts.push_back(0.5f);
                pinCircleVerts.push_back(1.0f);
                pinCircleVerts.push_back(alpha);
            }
        }

        if (!pinCircleVerts.empty()) {
            glDisable(GL_DEPTH_TEST);
            glm::mat4 ortho = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f);
            glUseProgram(lineShader);
            glUniformMatrix4fv(glGetUniformLocation(lineShader, "uVP"), 1, GL_FALSE, glm::value_ptr(ortho));

            glBindVertexArray(batchedLineVAO);
            glBindBuffer(GL_ARRAY_BUFFER, batchedLineVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, pinCircleVerts.size() * sizeof(float), pinCircleVerts.data());

            glEnable(GL_LINE_SMOOTH);
            glLineWidth(2.0f);
            glDrawArrays(GL_LINES, 0, pinCircleVerts.size() / 7);
            glLineWidth(1.0f);
            glEnable(GL_DEPTH_TEST);
        }
    }

    // Render URL labels in screen space with smooth transitions
    glDisable(GL_DEPTH_TEST);

    glm::mat4 ortho = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f);
    glUseProgram(textShader);
    glUniformMatrix4fv(glGetUniformLocation(textShader, "uProj"), 1, GL_FALSE, glm::value_ptr(ortho));
    glBindVertexArray(textVAO);

    // Sort nodes by size (biggest first) for label priority
    std::vector<size_t> sortedIndices(graph.nodes.size());
    for (size_t i = 0; i < graph.nodes.size(); i++) sortedIndices[i] = i;
    std::sort(sortedIndices.begin(), sortedIndices.end(), [&](size_t a, size_t b) {
        if ((int)a == selectedNode) return true;
        if ((int)b == selectedNode) return false;
        return graph.nodes[a].size > graph.nodes[b].size;
    });

    // Reset visibility flags for all labels
    for (auto& [idx, state] : labelStates) {
        state.visible = false;
    }

    // Track occupied screen regions for occlusion
    std::vector<std::tuple<float, float, float, float>> occupiedRects;

    // First pass: determine which labels should be visible and their target positions
    // Skip if labels are hidden (but still process fade-out below)
    for (size_t idx : sortedIndices) {
        if (!showLabels) break;
        const auto& node = graph.nodes[idx];
        if (node.fadeIn < 0.01f) continue;

        glm::vec3 screenPos = worldToScreen(node.position);
        if (screenPos.z < -1 || screenPos.z > 1) continue;

        float dist = glm::length(node.position - camera.position);
        float maxDist = 5.0f + node.size * node.size * 25.0f;
        if (dist > maxDist) continue;

        // Prepare label text
        std::string label = node.url;
        size_t protoEnd = label.find("://");
        if (protoEnd != std::string::npos) {
            label = label.substr(protoEnd + 3);
        }
        if (!label.empty() && label.back() == '/') {
            label.pop_back();
        }
        if ((int)idx != selectedNode && label.length() > 40) {
            label = label.substr(0, 37) + "...";
        } else if (label.length() > 100) {
            label = label.substr(0, 97) + "...";
        }
        if (node.status == NodeStatus::Error && node.httpCode != 0) {
            label += " - " + std::to_string(node.httpCode);
        }
        if (label.empty()) continue;

        // Estimate text size for occlusion check
        int estW = 0, estH = 0;
        if (font) TTF_SizeUTF8(font, label.c_str(), &estW, &estH);
        if (estW == 0) continue;

        float targetX = screenPos.x - estW / 2.0f;
        float targetY = screenPos.y + 15.0f;

        // Check overlap
        bool overlaps = false;
        if ((int)idx != selectedNode) {
            for (const auto& rect : occupiedRects) {
                float rx = std::get<0>(rect), ry = std::get<1>(rect);
                float rw = std::get<2>(rect), rh = std::get<3>(rect);
                if (targetX < rx + rw && targetX + estW > rx && targetY < ry + rh && targetY + estH > ry) {
                    overlaps = true;
                    break;
                }
            }
        }

        if (!overlaps) {
            occupiedRects.push_back({targetX, targetY, (float)estW, (float)estH});

            // Update or create label state
            auto& state = labelStates[(int)idx];
            state.visible = true;
            state.targetX = targetX;
            state.targetY = targetY;

            // Initialize position if this is a new label
            if (state.opacity < 0.01f) {
                state.x = targetX;
                state.y = targetY;
            }
        }
    }

    // Animation constants
    const float fadeSpeed = 6.0f;

    // Second pass: interpolate and render all labels with non-zero opacity
    std::vector<int> toRemove;
    for (auto& [idx, state] : labelStates) {
        // Interpolate opacity
        float targetOpacity = state.visible ? 1.0f : 0.0f;
        float opacityDiff = targetOpacity - state.opacity;
        if (std::abs(opacityDiff) > 0.001f) {
            state.opacity += opacityDiff * fadeSpeed * dt;
            state.opacity = std::max(0.0f, std::min(1.0f, state.opacity));
        } else {
            state.opacity = targetOpacity;
        }

        // Update position - always track the node
        // For fading out labels, recalculate target from node position
        if (!state.visible && idx >= 0 && idx < (int)graph.nodes.size()) {
            glm::vec3 screenPos = worldToScreen(graph.nodes[idx].position);
            int estW = 0, estH = 0;
            std::string label = graph.nodes[idx].url;
            size_t pe = label.find("://");
            if (pe != std::string::npos) label = label.substr(pe + 3);
            if (!label.empty() && label.back() == '/') label.pop_back();
            if (font) TTF_SizeUTF8(font, label.c_str(), &estW, &estH);
            state.targetX = screenPos.x - estW / 2.0f;
            state.targetY = screenPos.y + 15.0f;
        }

        // Snap to target (no camera lag) - smooth fade is enough
        state.x = state.targetX;
        state.y = state.targetY;

        // Skip rendering if fully faded out
        if (state.opacity < 0.01f) {
            if (!state.visible) toRemove.push_back(idx);
            continue;
        }

        // Node might have been deleted
        if (idx < 0 || idx >= (int)graph.nodes.size()) {
            toRemove.push_back(idx);
            continue;
        }

        const auto& node = graph.nodes[idx];

        // Prepare label text
        std::string label = node.url;
        size_t protoEnd = label.find("://");
        if (protoEnd != std::string::npos) {
            label = label.substr(protoEnd + 3);
        }
        if (!label.empty() && label.back() == '/') {
            label.pop_back();
        }
        if (idx != selectedNode && label.length() > 40) {
            label = label.substr(0, 37) + "...";
        } else if (label.length() > 100) {
            label = label.substr(0, 97) + "...";
        }
        if (node.status == NodeStatus::Error && node.httpCode != 0) {
            label += " - " + std::to_string(node.httpCode);
        }
        if (label.empty()) continue;

        auto cached = getTextTexture(label);
        if (!cached.tex) continue;

        float x = state.x;
        float y = state.y;

        glBindTexture(GL_TEXTURE_2D, cached.tex);

        float verts[] = {
            x,            y,            0.0f, 0.0f,
            x + cached.w, y,            1.0f, 0.0f,
            x + cached.w, y + cached.h, 1.0f, 1.0f,
            x,            y,            0.0f, 0.0f,
            x + cached.w, y + cached.h, 1.0f, 1.0f,
            x,            y + cached.h, 0.0f, 1.0f,
        };

        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

        glm::vec3 textColor = (idx == selectedNode) ? glm::vec3(1.0f, 1.0f, 0.4f) : glm::vec3(0.7f, 0.7f, 0.8f);
        textColor *= state.opacity * node.fadeIn;
        glUniform3fv(glGetUniformLocation(textShader, "uColor"), 1, glm::value_ptr(textColor));

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    // Clean up labels for deleted nodes
    for (int idx : toRemove) {
        labelStates.erase(idx);
    }

    glEnable(GL_DEPTH_TEST);
}

void Renderer::renderCrosshair(int screenW, int screenH) {
    glDisable(GL_DEPTH_TEST);
    glUseProgram(uiShader);

    glm::mat4 ortho = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f);
    glUniformMatrix4fv(glGetUniformLocation(uiShader, "uProj"), 1, GL_FALSE, glm::value_ptr(ortho));
    glUniform3f(glGetUniformLocation(uiShader, "uColor"), 1.0f, 1.0f, 1.0f);

    glBindVertexArray(quadVAO);

    float cx = screenW / 2.0f, cy = screenH / 2.0f;
    float size = 8.0f, thick = 1.0f;

    glUniform2f(glGetUniformLocation(uiShader, "uOffset"), cx - size, cy - thick/2);
    glUniform2f(glGetUniformLocation(uiShader, "uScale"), size * 2, thick);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glUniform2f(glGetUniformLocation(uiShader, "uOffset"), cx - thick/2, cy - size);
    glUniform2f(glGetUniformLocation(uiShader, "uScale"), thick, size * 2);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glEnable(GL_DEPTH_TEST);
}

void Renderer::renderAddressBar(const std::string& text, int screenW, int screenH, bool active) {
    if (!active) return;

    glDisable(GL_DEPTH_TEST);

    float barW = screenW - 100.0f;
    float barH = 30.0f;
    float barX = 50.0f;
    float barY = screenH - barH - 50.0f;
    float radius = 8.0f;

    // Rounded background
    glUseProgram(roundedShader);
    glm::mat4 ortho = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f);
    glUniformMatrix4fv(glGetUniformLocation(roundedShader, "uProj"), 1, GL_FALSE, glm::value_ptr(ortho));
    glBindVertexArray(quadVAO);

    glUniform3f(glGetUniformLocation(roundedShader, "uColor"), 0.12f, 0.12f, 0.18f);
    glUniform2f(glGetUniformLocation(roundedShader, "uOffset"), barX, barY);
    glUniform2f(glGetUniformLocation(roundedShader, "uScale"), barW, barH);
    glUniform2f(glGetUniformLocation(roundedShader, "uSize"), barW, barH);
    glUniform1f(glGetUniformLocation(roundedShader, "uRadius"), radius);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Render actual text
    if (!text.empty() && font) {
        glUseProgram(textShader);
        glUniformMatrix4fv(glGetUniformLocation(textShader, "uProj"), 1, GL_FALSE, glm::value_ptr(ortho));

        auto cached = getTextTexture(text);
        if (cached.tex) {
            glBindTexture(GL_TEXTURE_2D, cached.tex);
            glBindVertexArray(textVAO);

            float x = barX + 12.0f;
            float y = barY + (barH - cached.h) / 2.0f;

            float verts[] = {
                x,            y,            0.0f, 0.0f,
                x + cached.w, y,            1.0f, 0.0f,
                x + cached.w, y + cached.h, 1.0f, 1.0f,
                x,            y,            0.0f, 0.0f,
                x + cached.w, y + cached.h, 1.0f, 1.0f,
                x,            y + cached.h, 0.0f, 1.0f,
            };

            glBindBuffer(GL_ARRAY_BUFFER, textVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
            glUniform3f(glGetUniformLocation(textShader, "uColor"), 1.0f, 1.0f, 1.0f);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    }

    // Cursor
    glUseProgram(uiShader);
    glBindVertexArray(quadVAO);
    glUniformMatrix4fv(glGetUniformLocation(uiShader, "uProj"), 1, GL_FALSE, glm::value_ptr(ortho));

    float cursorX = barX + 12.0f;
    if (!text.empty() && font) {
        int w, h;
        TTF_SizeUTF8(font, text.c_str(), &w, &h);
        cursorX += w + 2;
    }

    glUniform3f(glGetUniformLocation(uiShader, "uColor"), 1.0f, 1.0f, 1.0f);
    glUniform2f(glGetUniformLocation(uiShader, "uOffset"), cursorX, barY + 5.0f);
    glUniform2f(glGetUniformLocation(uiShader, "uScale"), 2.0f, barH - 10.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glEnable(GL_DEPTH_TEST);
}

void Renderer::renderText2D(const std::string& text, float x, float y, glm::vec3 color) {
    // Utility for future use
}

void Renderer::renderStats(int screenW, int screenH, int nodeCount, int edgeCount, int pendingCount) {
    glDisable(GL_DEPTH_TEST);

    glm::mat4 ortho = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f);
    glUseProgram(textShader);
    glUniformMatrix4fv(glGetUniformLocation(textShader, "uProj"), 1, GL_FALSE, glm::value_ptr(ortho));
    glBindVertexArray(textVAO);

    std::string stats = std::to_string(nodeCount) + " nodes | " +
                        std::to_string(edgeCount) + " edges";
    if (pendingCount > 0) {
        stats += " | " + std::to_string(pendingCount) + " pending";
    }

    auto cached = getTextTexture(stats);
    if (cached.tex) {
        float x = 10.0f;
        float y = 10.0f;

        glBindTexture(GL_TEXTURE_2D, cached.tex);

        float verts[] = {
            x,            y,            0.0f, 0.0f,
            x + cached.w, y,            1.0f, 0.0f,
            x + cached.w, y + cached.h, 1.0f, 1.0f,
            x,            y,            0.0f, 0.0f,
            x + cached.w, y + cached.h, 1.0f, 1.0f,
            x,            y + cached.h, 0.0f, 1.0f,
        };

        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
        glUniform3f(glGetUniformLocation(textShader, "uColor"), 0.5f, 0.5f, 0.6f);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glEnable(GL_DEPTH_TEST);
}

void Renderer::renderVisibilityMenu(int screenW, int screenH, int selection, bool showNodes, bool showLinks, bool showLabels, bool domainColors, bool showStats) {
    glDisable(GL_DEPTH_TEST);

    float menuW = 220.0f;
    float menuH = 176.0f;
    float menuX = (screenW - menuW) / 2.0f;
    float menuY = (screenH - menuH) / 2.0f;
    float radius = 10.0f;
    float itemH = 28.0f;
    float padding = 12.0f;

    glm::mat4 ortho = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f);

    // Background
    glUseProgram(roundedShader);
    glUniformMatrix4fv(glGetUniformLocation(roundedShader, "uProj"), 1, GL_FALSE, glm::value_ptr(ortho));
    glBindVertexArray(quadVAO);

    glUniform3f(glGetUniformLocation(roundedShader, "uColor"), 0.08f, 0.08f, 0.12f);
    glUniform2f(glGetUniformLocation(roundedShader, "uOffset"), menuX, menuY);
    glUniform2f(glGetUniformLocation(roundedShader, "uScale"), menuW, menuH);
    glUniform2f(glGetUniformLocation(roundedShader, "uSize"), menuW, menuH);
    glUniform1f(glGetUniformLocation(roundedShader, "uRadius"), radius);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Menu items
    const char* labels[] = {"1. Nodes", "2. Links", "3. Labels", "4. Domain Colors", "5. Stats"};
    bool values[] = {showNodes, showLinks, showLabels, domainColors, showStats};

    glUseProgram(textShader);
    glUniformMatrix4fv(glGetUniformLocation(textShader, "uProj"), 1, GL_FALSE, glm::value_ptr(ortho));
    glBindVertexArray(textVAO);

    for (int i = 0; i < 5; i++) {
        float itemY = menuY + padding + i * itemH;

        // Selection highlight
        if (i == selection) {
            glUseProgram(roundedShader);
            glUniform3f(glGetUniformLocation(roundedShader, "uColor"), 0.2f, 0.2f, 0.3f);
            glUniform2f(glGetUniformLocation(roundedShader, "uOffset"), menuX + 6, itemY);
            glUniform2f(glGetUniformLocation(roundedShader, "uScale"), menuW - 12, itemH - 4);
            glUniform2f(glGetUniformLocation(roundedShader, "uSize"), menuW - 12, itemH - 4);
            glUniform1f(glGetUniformLocation(roundedShader, "uRadius"), 4.0f);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glUseProgram(textShader);
            glUniformMatrix4fv(glGetUniformLocation(textShader, "uProj"), 1, GL_FALSE, glm::value_ptr(ortho));
            glBindVertexArray(textVAO);
        }

        // Checkbox
        std::string checkbox = values[i] ? "[x] " : "[ ] ";
        std::string text = checkbox + labels[i];

        auto cached = getTextTexture(text);
        if (!cached.tex) continue;

        float x = menuX + padding;
        float y = itemY + (itemH - cached.h) / 2.0f;

        glBindTexture(GL_TEXTURE_2D, cached.tex);

        float verts[] = {
            x,            y,            0.0f, 0.0f,
            x + cached.w, y,            1.0f, 0.0f,
            x + cached.w, y + cached.h, 1.0f, 1.0f,
            x,            y,            0.0f, 0.0f,
            x + cached.w, y + cached.h, 1.0f, 1.0f,
            x,            y + cached.h, 0.0f, 1.0f,
        };

        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

        glm::vec3 color = (i == selection) ? glm::vec3(1.0f, 1.0f, 0.4f) : glm::vec3(0.8f, 0.8f, 0.9f);
        glUniform3fv(glGetUniformLocation(textShader, "uColor"), 1, glm::value_ptr(color));
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glEnable(GL_DEPTH_TEST);
}
