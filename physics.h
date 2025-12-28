#pragma once
#include "graph.h"

class Physics {
public:
    float springStrength = 2.0f;
    float repulsion = 20.0f;
    float drag = 4.0f;
    float maxSpeed = 10000.0f;

    void update(Graph& graph, float dt);
};
