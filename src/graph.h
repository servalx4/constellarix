#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

enum class NodeStatus { Pending, Success, Error };

struct Node {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    std::string url;
    int httpCode = 0; // 0 = pending, 200 = ok, 404 = not found, etc.
    std::vector<std::string> links;
    std::vector<int> childIndices;
    int parentIndex = -1;
    float size = 0.4f;
    float targetSize = 0.4f;
    float fadeIn = 0.0f; // 0 = invisible, 1 = fully visible
    NodeStatus status = NodeStatus::Pending;
    bool expanded = false;
    bool fetching = false;
    bool pinned = false;
};

struct Edge {
    int from, to;
    float restLength = 4.5f;
    float fadeIn = 0.0f; // 0 = invisible, 1 = fully visible
};

class Graph {
public:
    std::vector<Node> nodes;
    std::vector<Edge> edges;

    int addNode(const std::string& url, const glm::vec3& pos);
    void addEdge(int from, int to);
    bool hasEdge(int from, int to) const;
    int findNodeByUrl(const std::string& url) const;
    void deleteNode(int idx);
    void clear();
    int raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDist = 100.0f) const;
};
