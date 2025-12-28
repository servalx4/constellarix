#include "graph.h"
#include <algorithm>
#include <cmath>
#include <iostream>

int Graph::addNode(const std::string& url, const glm::vec3& pos) {
    Node n;
    n.url = url;
    n.position = pos;
    n.pinned = false;  // Explicitly ensure not pinned
    nodes.push_back(n);
    return nodes.size() - 1;
}

void Graph::addEdge(int from, int to) {
    if (from == to) return;
    if (from < 0 || from >= (int)nodes.size()) return;
    if (to < 0 || to >= (int)nodes.size()) return;
    if (hasEdge(from, to)) return;
    edges.push_back({from, to, 4.5f, 0.0f});
    nodes[from].childIndices.push_back(to);
}

bool Graph::hasEdge(int from, int to) const {
    for (const auto& e : edges) {
        if ((e.from == from && e.to == to) || (e.from == to && e.to == from)) {
            return true;
        }
    }
    return false;
}

int Graph::findNodeByUrl(const std::string& url) const {
    for (size_t i = 0; i < nodes.size(); i++) {
        if (nodes[i].url == url) return i;
    }
    return -1;
}

void Graph::deleteNode(int idx) {
    if (idx < 0 || idx >= (int)nodes.size()) return;

    // Remove edges involving this node
    edges.erase(std::remove_if(edges.begin(), edges.end(), [idx](const Edge& e) {
        return e.from == idx || e.to == idx;
    }), edges.end());

    // Update edge indices for nodes after the deleted one
    for (auto& e : edges) {
        if (e.from > idx) e.from--;
        if (e.to > idx) e.to--;
    }

    // Update childIndices in all nodes
    for (auto& n : nodes) {
        n.childIndices.erase(std::remove_if(n.childIndices.begin(), n.childIndices.end(), [idx](int i) {
            return i == idx;
        }), n.childIndices.end());
        for (auto& ci : n.childIndices) {
            if (ci > idx) ci--;
        }
    }

    nodes.erase(nodes.begin() + idx);
}

void Graph::clear() {
    nodes.clear();
    edges.clear();
}

int Graph::raycast(const glm::vec3& origin, const glm::vec3& dir, float maxDist) const {
    int closest = -1;
    float closestDist = maxDist;

    for (size_t i = 0; i < nodes.size(); i++) {
        glm::vec3 toNode = nodes[i].position - origin;
        float t = glm::dot(toNode, dir);
        if (t < 0) continue;

        glm::vec3 closestPoint = origin + dir * t;
        float dist = glm::length(nodes[i].position - closestPoint);
        float radius = nodes[i].size * 0.5f;

        if (dist < radius && t < closestDist) {
            closestDist = t;
            closest = i;
        }
    }
    return closest;
}
