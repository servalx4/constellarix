#include "window.h"
#include "camera.h"
#include "renderer.h"
#include "graph.h"
#include "physics.h"
#include "http_client.h"
#include "html_parser.h"
#include "ui.h"
#include <iostream>
#include <random>
#include <queue>
#include <unordered_map>

std::random_device rd;
std::mt19937 rng(rd());

// Queues for gradual link expansion (per-node so multiple can expand at once)
std::unordered_map<int, std::queue<std::string>> pendingLinksPerNode;
float linkSpawnTimer = 0.0f;
const float linkSpawnDelay = 0.01f; // 10ms between spawns
const float fadeSpeed = 3.0f; // fade in over ~0.3 seconds

glm::vec3 randomOffset(float radius) {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    glm::vec3 v(dist(rng), dist(rng), dist(rng));
    return glm::normalize(v) * radius;
}

void fetchNode(Graph* graphPtr, HttpClient& http, int nodeIdx) {
    if (nodeIdx < 0 || nodeIdx >= (int)graphPtr->nodes.size()) return;
    Node& node = graphPtr->nodes[nodeIdx];
    if (node.fetching) return;
    node.fetching = true;

    std::string url = node.url;
    http.fetchAsync(url, [graphPtr, nodeIdx, url](HttpResponse resp) {
        Graph& graph = *graphPtr;
        // Validate node still exists and has same URL (graph might have been cleared)
        if (nodeIdx < 0 || nodeIdx >= (int)graph.nodes.size()) return;
        if (graph.nodes[nodeIdx].url != url) return;

        Node& node = graph.nodes[nodeIdx];
        node.fetching = false;

        if (!resp.error.empty() || resp.statusCode >= 400) {
            node.status = NodeStatus::Error;
            node.httpCode = resp.statusCode > 0 ? resp.statusCode : -1;
            std::cout << "Error fetching " << url << ": " << node.httpCode << "\n";
        } else {
            node.status = NodeStatus::Success;
            auto links = extractLinks(resp.body, url);
            if (links.size() > 200) links.resize(200);
            node.links = std::move(links);
            float logSize = std::log(1.0f + node.links.size());
            node.targetSize = 0.4f + 0.25f * logSize * (1.0f + logSize * 0.1f);
            std::cout << "Fetched " << url << " - " << node.links.size() << " links\n";
        }
    });
}

void activateNode(Graph& graph, HttpClient& http, int nodeIdx) {
    if (nodeIdx < 0 || nodeIdx >= (int)graph.nodes.size()) return;
    Node& node = graph.nodes[nodeIdx];

    if (node.status == NodeStatus::Pending) {
        // Still fetching, do nothing
        return;
    }

    if (node.status == NodeStatus::Error) {
        // Retry failed request
        node.status = NodeStatus::Pending;
        node.fetching = false;
        fetchNode(&graph, http, nodeIdx);
        std::cout << "Retrying: " << node.url << "\n";
        return;
    }

    // Success - expand if not already
    if (node.expanded) return;
    node.expanded = true;

    // Queue all links for gradual expansion (per-node queue)
    auto& queue = pendingLinksPerNode[nodeIdx];
    for (const auto& link : node.links) {
        queue.push(link);
    }
    std::cout << "Queued: " << node.url << " (" << node.links.size() << " links)\n";
}

void processPendingLinks(Graph& graph, HttpClient& http, float dt) {
    // Update fade-in and size interpolation for all nodes
    const float sizeSpeed = 4.0f; // smooth size transitions
    for (size_t i = 0; i < graph.nodes.size(); i++) {
        if (graph.nodes[i].fadeIn < 1.0f) {
            graph.nodes[i].fadeIn = std::min(1.0f, graph.nodes[i].fadeIn + fadeSpeed * dt);
        }
        // Smooth size interpolation
        float diff = graph.nodes[i].targetSize - graph.nodes[i].size;
        if (std::abs(diff) > 0.001f) {
            graph.nodes[i].size += diff * sizeSpeed * dt;
        } else {
            graph.nodes[i].size = graph.nodes[i].targetSize;
        }
    }
    for (size_t i = 0; i < graph.edges.size(); i++) {
        if (graph.edges[i].fadeIn < 1.0f) {
            graph.edges[i].fadeIn = std::min(1.0f, graph.edges[i].fadeIn + fadeSpeed * dt);
        }
    }

    if (pendingLinksPerNode.empty()) return;

    linkSpawnTimer += dt;
    if (linkSpawnTimer < linkSpawnDelay) return;
    linkSpawnTimer -= linkSpawnDelay;

    // Process one link from each active node queue (copy keys to avoid iterator issues)
    std::vector<int> parentKeys;
    for (auto& [k, v] : pendingLinksPerNode) {
        parentKeys.push_back(k);
    }

    for (int parentIdx : parentKeys) {
        auto it = pendingLinksPerNode.find(parentIdx);
        if (it == pendingLinksPerNode.end()) continue;

        auto& queue = it->second;
        if (queue.empty()) {
            pendingLinksPerNode.erase(parentIdx);
            continue;
        }

        // Validate parent still exists
        if (parentIdx < 0 || parentIdx >= (int)graph.nodes.size()) {
            pendingLinksPerNode.erase(parentIdx);
            continue;
        }

        std::string url = queue.front();
        queue.pop();

        if (queue.empty()) {
            pendingLinksPerNode.erase(parentIdx);
        }

        // Check if this URL already exists as a node
        int existingIdx = graph.findNodeByUrl(url);
        if (existingIdx >= 0) {
            // Link to existing node
            graph.addEdge(parentIdx, existingIdx);
        } else {
            // Create new node - get position before modifying graph
            glm::vec3 parentPos = graph.nodes[parentIdx].position;
            glm::vec3 pos = parentPos + randomOffset(6.0f);
            int childIdx = graph.addNode(url, pos);
            graph.addEdge(parentIdx, childIdx);
            fetchNode(&graph, http, childIdx);
        }
    }
}

int main(int argc, char* argv[]) {
    int width = 1280, height = 720;

    // Parse args: -w WIDTH -h HEIGHT or WIDTHxHEIGHT
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-w" && i + 1 < argc) {
            width = std::stoi(argv[++i]);
        } else if (arg == "-h" && i + 1 < argc) {
            height = std::stoi(argv[++i]);
        } else if (arg.find('x') != std::string::npos) {
            size_t x = arg.find('x');
            width = std::stoi(arg.substr(0, x));
            height = std::stoi(arg.substr(x + 1));
        }
    }

    Window window;
    if (!window.init(width, height, "Constellarix")) {
        return 1;
    }

    Renderer renderer;
    if (!renderer.init()) {
        return 1;
    }

    Camera camera;
    Graph graph;
    graph.nodes.reserve(1000);
    graph.edges.reserve(5000);
    Physics physics;
    HttpClient http;
    UI ui;

    Uint64 lastTime = SDL_GetPerformanceCounter();
    float freq = (float)SDL_GetPerformanceFrequency();

    std::cout << "Controls:\n";
    std::cout << "  WASD + Mouse - Move and look\n";
    std::cout << "  Shift - Move faster\n";
    std::cout << "  Enter - Open address bar, type URL, Enter to submit\n";
    std::cout << "  Esc - Cancel address bar\n";
    std::cout << "  E - Expand selected node (show links)\n";
    std::cout << "  X - Expand all nodes (explosive)\n";
    std::cout << "  Q - Delete selected node\n";
    std::cout << "  R - Visibility menu (toggle nodes/links/labels)\n";
    std::cout << "  Left Click - Drag node\n";
    std::cout << "  Right Click - Pin/unpin node (lock position)\n";
    std::cout << "  Delete - Clear all nodes\n";
    std::cout << "  F11 - Toggle fullscreen\n";
    std::cout << "  Ctrl+Q - Quit\n\n";

    int draggingNode = -1;
    float dragDistance = 0.0f;
    glm::vec3 lastDragPos(0.0f);
    glm::vec3 dragVelocity(0.0f);

    while (!window.shouldClose()) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (now - lastTime) / freq;
        lastTime = now;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                window.close();
            }

            ui.handleEvent(event);

            // F11 toggles fullscreen (works even when typing)
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
                window.toggleFullscreen();
            }

            if (!ui.addressBarActive) {
                if (event.type == SDL_KEYDOWN) {
                    if (event.key.keysym.sym == SDLK_q && (event.key.keysym.mod & KMOD_CTRL)) {
                        window.close();
                    } else if (event.key.keysym.sym == SDLK_q) {
                        int selected = graph.raycast(camera.position, camera.getForward());
                        if (selected >= 0) {
                            pendingLinksPerNode.erase(selected);
                            graph.deleteNode(selected);
                        }
                    } else if (event.key.keysym.sym == SDLK_DELETE || event.key.keysym.sym == SDLK_BACKSPACE) {
                        graph.clear();
                        pendingLinksPerNode.clear();
                        std::cout << "Cleared all nodes\n";
                    }
                } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && !ui.menuOpen) {
                    int selected = graph.raycast(camera.position, camera.getForward());
                    if (selected >= 0) {
                        draggingNode = selected;
                        dragDistance = glm::length(graph.nodes[selected].position - camera.position);
                        lastDragPos = graph.nodes[selected].position;
                        dragVelocity = glm::vec3(0.0f);
                    }
                } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                    if (draggingNode >= 0 && draggingNode < (int)graph.nodes.size()) {
                        graph.nodes[draggingNode].velocity = dragVelocity;
                    }
                    draggingNode = -1;
                } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT && !ui.menuOpen) {
                    int selected = graph.raycast(camera.position, camera.getForward());
                    if (selected >= 0) {
                        graph.nodes[selected].pinned = !graph.nodes[selected].pinned;
                        std::cout << (graph.nodes[selected].pinned ? "Pinned: " : "Unpinned: ") << graph.nodes[selected].url << "\n";
                    }
                } else if (event.type == SDL_MOUSEMOTION && !ui.menuOpen) {
                    camera.processMouse(event.motion.xrel, event.motion.yrel);
                }
            }
        }

        // Handle URL submission
        if (ui.hasSubmittedUrl()) {
            std::string url = ui.consumeSubmittedUrl();
            glm::vec3 spawnPos = camera.position + camera.getForward() * 5.0f;
            int nodeIdx = graph.addNode(url, spawnPos);
            fetchNode(&graph, http, nodeIdx);
            std::cout << "Added node: " << url << "\n";
        }

        // Keyboard input for movement
        if (!ui.addressBarActive && !ui.menuOpen) {
            const uint8_t* keys = SDL_GetKeyboardState(nullptr);
            camera.processKeyboard(keys, dt);

            // Check for E to activate node (expand or retry)
            static bool eWasPressed = false;
            if (keys[SDL_SCANCODE_E] && !eWasPressed) {
                int selected = graph.raycast(camera.position, camera.getForward());
                if (selected >= 0) {
                    activateNode(graph, http, selected);
                }
            }
            eWasPressed = keys[SDL_SCANCODE_E];

            // Check for X to expand all nodes (explosive)
            static bool xWasPressed = false;
            if (keys[SDL_SCANCODE_X] && !xWasPressed) {
                int count = graph.nodes.size();
                for (int i = 0; i < count; i++) {
                    activateNode(graph, http, i);
                }
                std::cout << "Expanding all " << count << " nodes\n";
            }
            xWasPressed = keys[SDL_SCANCODE_X];
        }

        // Drag node - keep at same distance, move with crosshair
        if (draggingNode >= 0 && draggingNode < (int)graph.nodes.size()) {
            glm::vec3 newPos = camera.position + camera.getForward() * dragDistance;
            // Track velocity for throwing
            if (dt > 0.0001f) {
                dragVelocity = (newPos - lastDragPos) / dt;
            }
            lastDragPos = newPos;
            graph.nodes[draggingNode].position = newPos;
            graph.nodes[draggingNode].velocity = glm::vec3(0.0f); // Stop physics while dragging
        }

        // Update
        http.update();
        processPendingLinks(graph, http, dt);
        physics.update(graph, dt);

        // Find selected node for highlighting
        int selectedNode = -1;
        if (!ui.addressBarActive) {
            selectedNode = graph.raycast(camera.position, camera.getForward());
        }

        // Render
        int sw = window.getWidth(), sh = window.getHeight();
        renderer.begin(camera, sw, sh);
        renderer.renderGraph(graph, selectedNode, camera, sw, sh, dt, ui.showNodes, ui.showLinks, ui.showLabels, ui.domainColors);
        renderer.renderCrosshair(sw, sh);

        // Stats display (if enabled)
        if (ui.showStats) {
            int pendingCount = 0;
            for (const auto& [k, q] : pendingLinksPerNode) {
                pendingCount += q.size();
            }
            renderer.renderStats(sw, sh, graph.nodes.size(), graph.edges.size(), pendingCount);
        }

        renderer.renderAddressBar(ui.addressBarText, sw, sh, ui.addressBarActive);
        if (ui.menuOpen) {
            renderer.renderVisibilityMenu(sw, sh, ui.menuSelection, ui.showNodes, ui.showLinks, ui.showLabels, ui.domainColors, ui.showStats);
        }

        window.swap();
    }

    renderer.shutdown();
    window.shutdown();
    return 0;
}
