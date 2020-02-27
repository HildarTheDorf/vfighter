#include "Mesh.hpp"

Mesh::Mesh(const char *filename)
{
    tinyobj::attrib_t _attribs;
    std::vector<tinyobj::shape_t> _shapes;
    tinyobj::LoadObj(&_attribs, &_shapes, nullptr, nullptr, filename);

    for (auto i = 0; i < _shapes[0].mesh.indices.size(); i += 1)
    {
        PerVertex vertex;
        for (auto j = 0; j < 3; ++j)
        {
            vertex.position[j] = _attribs.vertices[3 * _shapes[0].mesh.indices[i].vertex_index + j];
            vertex.normal[j] = _attribs.normals[3 * _shapes[0].mesh.indices[i].normal_index + j];
        }
        verticies.emplace_back(std::move(vertex));
    }
}
