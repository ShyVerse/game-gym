#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "renderer/gltf_loader.h"
#include "renderer/gpu_context.h"
#include "renderer/mesh.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace gg {
namespace {

/// Read all indices from a cgltf accessor into a uint32_t vector.
std::vector<uint32_t> read_indices(const cgltf_accessor* accessor) {
    std::vector<uint32_t> indices(accessor->count);
    for (cgltf_size i = 0; i < accessor->count; ++i) {
        indices[i] =
            static_cast<uint32_t>(cgltf_accessor_read_index(accessor, i));
    }
    return indices;
}

/// Read float data from a cgltf accessor.
std::vector<float> read_floats(const cgltf_accessor* accessor,
                               cgltf_size components) {
    const cgltf_size total = accessor->count * components;
    std::vector<float> out(total);
    cgltf_accessor_unpack_floats(accessor, out.data(), total);
    return out;
}

/// Generate flat (face) normals from positions and triangle indices.
std::vector<float> generate_flat_normals(const std::vector<float>& positions,
                                         const std::vector<uint32_t>& indices) {
    std::vector<float> normals(positions.size(), 0.0f);
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const uint32_t i0 = indices[i + 0];
        const uint32_t i1 = indices[i + 1];
        const uint32_t i2 = indices[i + 2];

        const float ax = positions[i1 * 3 + 0] - positions[i0 * 3 + 0];
        const float ay = positions[i1 * 3 + 1] - positions[i0 * 3 + 1];
        const float az = positions[i1 * 3 + 2] - positions[i0 * 3 + 2];

        const float bx = positions[i2 * 3 + 0] - positions[i0 * 3 + 0];
        const float by = positions[i2 * 3 + 1] - positions[i0 * 3 + 1];
        const float bz = positions[i2 * 3 + 2] - positions[i0 * 3 + 2];

        float nx = ay * bz - az * by;
        float ny = az * bx - ax * bz;
        float nz = ax * by - ay * bx;

        const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 1e-8f) {
            nx /= len;
            ny /= len;
            nz /= len;
        }

        for (int k = 0; k < 3; ++k) {
            const uint32_t idx = indices[i + static_cast<size_t>(k)];
            normals[idx * 3 + 0] = nx;
            normals[idx * 3 + 1] = ny;
            normals[idx * 3 + 2] = nz;
        }
    }
    return normals;
}

} // namespace

std::vector<std::unique_ptr<Mesh>> GltfLoader::load(const std::string& path,
                                                     GpuContext& ctx) {
    std::vector<std::unique_ptr<Mesh>> result;

    cgltf_options options{};
    cgltf_data* data = nullptr;

    if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success) {
        std::fprintf(stderr, "GltfLoader: failed to parse '%s'\n",
                     path.c_str());
        return result;
    }

    if (cgltf_load_buffers(&options, data, path.c_str()) !=
        cgltf_result_success) {
        std::fprintf(stderr, "GltfLoader: failed to load buffers for '%s'\n",
                     path.c_str());
        cgltf_free(data);
        return result;
    }

    for (cgltf_size mi = 0; mi < data->meshes_count; ++mi) {
        const cgltf_mesh& mesh = data->meshes[mi];
        for (cgltf_size pi = 0; pi < mesh.primitives_count; ++pi) {
            const cgltf_primitive& prim = mesh.primitives[pi];
            if (prim.type != cgltf_primitive_type_triangles) {
                continue;
            }

            // --- Locate attributes ---
            const cgltf_accessor* pos_acc = nullptr;
            const cgltf_accessor* norm_acc = nullptr;
            const cgltf_accessor* uv_acc = nullptr;

            for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai) {
                const cgltf_attribute& attr = prim.attributes[ai];
                if (attr.type == cgltf_attribute_type_position) {
                    pos_acc = attr.data;
                } else if (attr.type == cgltf_attribute_type_normal) {
                    norm_acc = attr.data;
                } else if (attr.type == cgltf_attribute_type_texcoord) {
                    uv_acc = attr.data;
                }
            }

            if (pos_acc == nullptr) {
                continue; // No positions — skip this primitive.
            }

            const cgltf_size vertex_count = pos_acc->count;

            // --- Positions (required) ---
            std::vector<float> positions = read_floats(pos_acc, 3);

            // --- Indices ---
            std::vector<uint32_t> indices;
            if (prim.indices != nullptr) {
                indices = read_indices(prim.indices);
            } else {
                // Generate sequential indices.
                indices.resize(vertex_count);
                for (cgltf_size i = 0; i < vertex_count; ++i) {
                    indices[i] = static_cast<uint32_t>(i);
                }
            }

            // --- Normals (optional — generate flat normals if missing) ---
            std::vector<float> normals;
            if (norm_acc != nullptr) {
                normals = read_floats(norm_acc, 3);
            } else {
                normals = generate_flat_normals(positions, indices);
            }

            // --- UVs (optional — fill zeros if missing) ---
            std::vector<float> uvs;
            if (uv_acc != nullptr) {
                uvs = read_floats(uv_acc, 2);
            } else {
                uvs.resize(vertex_count * 2, 0.0f);
            }

            // --- Build interleaved Vertex array ---
            std::vector<Vertex> vertices(vertex_count);
            for (cgltf_size vi = 0; vi < vertex_count; ++vi) {
                Vertex& v = vertices[vi];
                v.position[0] = positions[vi * 3 + 0];
                v.position[1] = positions[vi * 3 + 1];
                v.position[2] = positions[vi * 3 + 2];
                v.normal[0] = normals[vi * 3 + 0];
                v.normal[1] = normals[vi * 3 + 1];
                v.normal[2] = normals[vi * 3 + 2];
                v.uv[0] = uvs[vi * 2 + 0];
                v.uv[1] = uvs[vi * 2 + 1];
            }

            auto mesh_ptr = Mesh::create(ctx, vertices, indices);
            if (mesh_ptr) {
                result.push_back(std::move(mesh_ptr));
            }
        }
    }

    cgltf_free(data);
    return result;
}

} // namespace gg
