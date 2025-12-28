#include "physics.h"
#include <cmath>
#include <iostream>

void Physics::update(Graph& graph, float dt) {
    auto& nodes = graph.nodes;
    auto& edges = graph.edges;
    size_t n = nodes.size();

    // Repulsion with soft falloff
    const float maxRepulsionDist = 15.0f;

    for (size_t i = 0; i < n; i++) {
        for (size_t j = i + 1; j < n; j++) {
            glm::vec3 diff = nodes[i].position - nodes[j].position;
            float dist = glm::length(diff);

            if (dist > maxRepulsionDist) continue;
            if (dist < 0.5f) dist = 0.5f; // Soft minimum

            // Combined mass - bigger nodes (more links) repel stronger
            float combinedMass = (nodes[i].size + nodes[j].size) * 0.5f;

            // Inverse linear falloff (gentler than inverse square)
            float strength = repulsion * combinedMass / (dist * dist + 1.0f);
            glm::vec3 force = (diff / dist) * strength * dt;

            if (!nodes[i].pinned) nodes[i].velocity += force;
            if (!nodes[j].pinned) nodes[j].velocity -= force;
        }
    }

    // Spring forces on edges - stronger for bigger nodes (like gravity)
    for (auto& edge : edges) {
        if (edge.from < 0 || edge.from >= (int)n) continue;
        if (edge.to < 0 || edge.to >= (int)n) continue;
        Node& a = nodes[edge.from];
        Node& b = nodes[edge.to];

        glm::vec3 diff = b.position - a.position;
        float dist = glm::length(diff);
        if (dist < 0.1f) continue;

        // Combined mass based on node sizes (more links = bigger = more pull)
        float combinedMass = (a.size + b.size) * 0.5f;
        // Force weakens with distance
        float distanceFactor = 1.0f / (1.0f + dist * 0.1f);

        float displacement = dist - edge.restLength;
        glm::vec3 force = (diff / dist) * displacement * springStrength * combinedMass * distanceFactor * dt;

        if (!a.pinned) a.velocity += force;
        if (!b.pinned) b.velocity -= force;
    }

    // Apply drag and integrate
    for (auto& node : nodes) {
        if (node.pinned) {
            node.velocity = glm::vec3(0.0f); // Stop pinned nodes
            continue;
        }

        // Drag force opposes velocity, proportional to speed
        node.velocity -= node.velocity * drag * dt;

        float speed = glm::length(node.velocity);
        if (speed > maxSpeed) {
            node.velocity = (node.velocity / speed) * maxSpeed;
        }

        node.position += node.velocity * dt;
    }
}
