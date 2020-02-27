#pragma once

#include <glm/glm.hpp>
#include <tiny_obj_loader.h>

struct PerVertex
{
    glm::vec3 position;
    glm::vec3 normal;
};

struct Mesh
{
    explicit Mesh(const char *filenmae);

    std::vector<PerVertex> verticies;
};