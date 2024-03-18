#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <unordered_map>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "std_image.h"

#include "blocks.h"
#include "generation.h"
#include "player.h"
#include "types.h"
#include "world.h"

using namespace std;

// clang-format off
std::vector<float> cube_vertices = {
    -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 
     1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, 
     1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f, 
    -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, 
    -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, 
     1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, 
     1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, 
    -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f
};

std::vector<unsigned int> cube_indices = {
    0, 1, 3,
    1, 2, 3,
    1, 2, 5,
    2, 5, 6,
    0, 1, 4,
    4, 1, 5,
    3, 0, 7,
    7, 0, 4,
    2, 3, 7,
    6, 2, 7
};

std::vector<float> fullscreen_rect_vertices = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
     1.0f,  1.0f,
    -1.0f,  1.0f,
};

std::vector<unsigned int> fullscreen_rect_indices = {
    0, 1, 3,
    1, 2, 3
};
// clang-format on

#define LAND_GREEN                                  \
    {                                               \
        124.f / 255.f, 189.f / 255.f, 107.f / 255.f \
    }

// TOP, FRONT, LEFT, BACK, RIGHT, BOTTOM

const int TEXTURE_BLOCKS_WIDTH = 1024;
const int TEXTURE_BLOCKS_HEIGHT = 512;
const float TEXTURE_TILE_WIDTH = 16.f / TEXTURE_BLOCKS_WIDTH;
const float TEXTURE_TILE_HEIGHT = 16.f / TEXTURE_BLOCKS_HEIGHT;

void init_slice(Slice_t *slice)
{
    slice->table = {};
}

void init_chunk(Chunk_t *chunk, Section_t *section, int64_t x, int64_t y)
{
    chunk->section_x = x;
    chunk->section_y = y;
    chunk->x = section->x * 256 + 16 * x;
    chunk->y = section->y * 256 + 16 * y;
    for (uint8_t slice = 0; slice < 24; slice++)
    {
        init_slice(&chunk->slices[slice]);
    }
}

inline int positive_mod(int i, int n)
{
    return (i % n + n) % n;
}

Block_t *get_block(Section_t *section, Chunk_t *chunk, Slice_t *slice, int32_t x, int32_t y, int32_t z)
{
    int32_t slice_x = positive_mod(x, 16);
    int32_t slice_y = positive_mod(y, 16);
    int32_t slice_z = positive_mod(z, 16);
    Slice_t *concerned_slice = slice;
    if (x < 0)
    {
        if (chunk->section_x <= 0)
            return &block_air;
        Chunk_t *concerned_chunk = section->chunks[chunk_id(chunk->section_x - 1, chunk->section_y)];
        if (concerned_chunk == NULL)
            return &block_air;
        concerned_slice = &concerned_chunk->slices[slice->index];
        if (concerned_slice == NULL)
            return &block_air;
    }
    if (x > 15)
    {
        if (chunk->section_x >= 15)
            return &block_air;
        Chunk_t *concerned_chunk = section->chunks[chunk_id(chunk->section_x + 1, chunk->section_y)];
        if (concerned_chunk == NULL)
            return &block_air;
        concerned_slice = &concerned_chunk->slices[slice->index];
        if (concerned_slice == NULL)
            return &block_air;
    }
    if (y < 0)
    {
        if (chunk->section_y <= 0)
            return &block_air;
        Chunk_t *concerned_chunk = section->chunks[chunk_id(chunk->section_x, chunk->section_y - 1)];
        if (concerned_chunk == NULL)
            return &block_air;
        concerned_slice = &concerned_chunk->slices[slice->index];
        if (concerned_slice == NULL)
            return &block_air;
    }
    if (y > 15)
    {
        if (chunk->section_y >= 15)
            return &block_air;
        Chunk_t *concerned_chunk = section->chunks[chunk_id(chunk->section_x, chunk->section_y + 1)];
        if (concerned_chunk == NULL)
            return &block_air;
        concerned_slice = &concerned_chunk->slices[slice->index];
        if (concerned_slice == NULL)
            return &block_air;
    }
    if (z < 0)
    {
        if (slice->index <= 0)
            return &block_air;
        concerned_slice = &chunk->slices[slice->index - 1];
        if (concerned_slice == NULL)
            return &block_air;
    }
    if (z > 15)
    {
        if (slice->index >= 23)
            return &block_air;
        concerned_slice = &chunk->slices[slice->index + 1];
        if (concerned_slice == NULL)
            return &block_air;
    }
    const size_t blocks_count = concerned_slice->table.size();
    if (blocks_count == 0)
    {
        std::cout << "[ERROR] Trying to get the block id of a block within an empty chunk" << std::endl;
        return 0;
    }
    else if (blocks_count == 1)
    {
        return &concerned_slice->table[0];
    }
    else
    {
        return &concerned_slice->table[concerned_slice->blocks[block_index(slice_x, slice_y, slice_z)]];
    }
}

void generate_chunk(World_t *world, Chunk_t *chunk)
{
    for (size_t slice_index = 0; slice_index < 24; slice_index++)
    {
        Slice_t *slice = &chunk->slices[slice_index];
        slice->table.push_back(block_air);              // AIR
        slice->table.push_back(Block_t{1});             // STONE
        slice->table.push_back(Block_t{2});             // DIRT
        slice->table.push_back(Block_t{3, LAND_GREEN}); // GRASS
        slice->table.push_back(Block_t{4});             // BEDROCK
        slice->table.push_back(Block_t{5});             // OAK LOG
        slice->table.push_back(Block_t{6, LAND_GREEN}); // OAK LEAVE
        slice->index = slice_index;
        slice->z = 16 * slice_index;
        for (size_t x = 0; x < 16; x++)
        {
            double block_x = x + chunk->x;
            for (size_t y = 0; y < 16; y++)
            {
                double block_y = y + chunk->y;
                float scale = 0.1f;
                uint8_t height = sample_perlin(&world->heightmap, block_x, block_y, 0.f);
                // uint8_t height = 50.f +
                //                  stb_perlin_noise3(scale * block_x, scale * block_y, 0.f, 0, 0, 0) * 5.f +
                //                  stb_perlin_noise3(0.2f * scale * block_x, 0.2f * scale * block_y, 0.f, 0, 0, 0) * 10.f;
                uint8_t dirt_height = (0.5f + 0.5f * stb_perlin_noise3(scale * block_x, scale * block_y, 0.f, 0, 0, 0)) * 5.f;
                for (size_t z = 0; z < 16; z++)
                {
                    int block_z = z + slice->z;
                    const float r = (float)rand() / (float)RAND_MAX;
                    bool bedrock = block_z == 0 || (block_z / 3.f) * (block_z / 3.f) < r;
                    if (bedrock)
                    {
                        slice->blocks[block_index(x, y, z)] = 4;
                        continue;
                    }
                    float scale = 0.05f;
                    float cave = stb_perlin_noise3(scale * block_x, scale * block_y, scale * block_z, 0, 0, 0);
                    bool air = block_z > height || cave >= 0.4f;
                    if (air)
                    {
                        slice->blocks[block_index(x, y, z)] = 0;
                    }
                    else
                    {
                        bool top_layer = block_z == height;
                        bool dirt_zone = (height - block_z) <= dirt_height;
                        slice->blocks[block_index(x, y, z)] = top_layer ? 3 : (dirt_zone ? 2 : 1);
                    }
                }
            }
        }
    }
}

void fill_rect(World_t *world, glm::vec3 min, glm::vec3 max, int16_t val)
{
    for (int64_t x = min.x; x <= max.x; x++)
    {
        for (int64_t y = min.y; y <= max.y; y++)
        {
            for (int64_t z = min.z; z <= max.z; z++)
            {
                uint16_t *block = get_global_block(world, x, y, z);
                if (block != NULL)
                    *block = val;
            }
        }
    }
}

void spawn_tree(World_t *world, glm::vec3 position, uint32_t height)
{
    glm::vec3 top = position + glm::vec3(0, 0, height);
    fill_rect(world, top - glm::vec3(2, 2, 2), top + glm::vec3(2, 2, 2), 6);
    for (size_t i = 0; i < height; i++)
    {
        uint16_t *block = get_global_block(world, position.x, position.y, position.z + i);
        if (block != NULL)
            *block = 5; // WOOD
    }
}

void push_indices(std::vector<unsigned int> *indices, size_t offset, float normal_direction)
{
    if (normal_direction > 0)
    {
        indices->push_back(offset + 2);
        indices->push_back(offset + 1);
        indices->push_back(offset + 0);
        indices->push_back(offset + 3);
        indices->push_back(offset + 2);
        indices->push_back(offset + 0);
    }
    else
    {
        indices->push_back(offset + 0);
        indices->push_back(offset + 1);
        indices->push_back(offset + 2);
        indices->push_back(offset + 0);
        indices->push_back(offset + 2);
        indices->push_back(offset + 3);
    }
}

void add_face_x(std::vector<float> *vertices, std::vector<unsigned int> *indices, int64_t x, int64_t y, int64_t z, float slice_height, float normal_direction, float uv_x, float uv_y, float uv_ov_x, float uv_ov_y, glm::vec3 tint = {1., 1., 1.})
{
    size_t offset = vertices->size() / 13;
    // clang-format off
    const std::vector<float> new_vertices = {
        (float)x, (float)y, (float)z, (float)normal_direction, 0.f, 0.f, 0.f+uv_x, TEXTURE_TILE_HEIGHT+uv_y, 0.f+uv_ov_x, TEXTURE_TILE_HEIGHT+uv_ov_y, tint.r, tint.g, tint.b,
        (float)x, (float)y+1.f, (float)z, (float)normal_direction, 0.f, 0.f, TEXTURE_TILE_WIDTH+uv_x, TEXTURE_TILE_HEIGHT+uv_y, TEXTURE_TILE_WIDTH+uv_ov_x, TEXTURE_TILE_HEIGHT+uv_ov_y, tint.r, tint.g, tint.b,
        (float)x, (float)y+1.f, (float)z+1.f, (float)normal_direction, 0.f, 0.f, TEXTURE_TILE_WIDTH+uv_x, 0.f+uv_y, TEXTURE_TILE_WIDTH+uv_ov_x, 0.f+uv_ov_y, tint.r, tint.g, tint.b,
        (float)x, (float)y, (float)z+1.f, (float)normal_direction, 0.f, 0.f, 0.f+uv_x, 0.f+uv_y, 0.f+uv_ov_x, 0.f+uv_ov_y, tint.r, tint.g, tint.b
    };
    // clang-format on

    vertices->reserve(vertices->size() + distance(new_vertices.begin(), new_vertices.end()));
    vertices->insert(vertices->end(), new_vertices.begin(), new_vertices.end());

    push_indices(indices, offset, -normal_direction);
}

void add_face_y(std::vector<float> *vertices, std::vector<unsigned int> *indices, int64_t x, int64_t y, int64_t z, float slice_height, float normal_direction, float uv_x, float uv_y, float uv_ov_x, float uv_ov_y, glm::vec3 tint = {1., 1., 1.})
{
    size_t offset = vertices->size() / 13;
    // clang-format off
    const std::vector<float> new_vertices = {
        (float)x, (float)y, (float)z, 0.f, (float)normal_direction, 0.f, 0.f+uv_x, TEXTURE_TILE_HEIGHT+uv_y, 0.f+uv_ov_x, TEXTURE_TILE_HEIGHT+uv_ov_y, tint.r, tint.g, tint.b,
        (float)x+1.f, (float)y, (float)z, 0.f, (float)normal_direction, 0.f, TEXTURE_TILE_WIDTH+uv_x, TEXTURE_TILE_HEIGHT+uv_y, TEXTURE_TILE_WIDTH+uv_ov_x, TEXTURE_TILE_HEIGHT+uv_ov_y, tint.r, tint.g, tint.b,
        (float)x+1.f, (float)y, (float)z+1.f, 0.f, (float)normal_direction, 0.f, TEXTURE_TILE_WIDTH+uv_x, 0.f+uv_y, TEXTURE_TILE_WIDTH+uv_ov_x, 0.f+uv_ov_y, tint.r, tint.g, tint.b,
        (float)x, (float)y, (float)z+1.f, 0.f, (float)normal_direction, 0.f, 0.f+uv_x, 0.f+uv_y, 0.f+uv_ov_x, 0.f+uv_ov_y, tint.r, tint.g, tint.b
    };
    // clang-format on

    vertices->reserve(vertices->size() + std::distance(new_vertices.begin(), new_vertices.end()));
    vertices->insert(vertices->end(), new_vertices.begin(), new_vertices.end());

    push_indices(indices, offset, normal_direction);
}

void add_face_z(std::vector<float> *vertices, std::vector<unsigned int> *indices, int64_t x, int64_t y, int64_t z, float slice_height, float normal_direction, float uv_x, float uv_y, float uv_ov_x, float uv_ov_y, glm::vec3 tint = {1., 1., 1.})
{
    size_t offset = vertices->size() / 13;
    // clang-format off
    const std::vector<float> new_vertices = {
        (float)x, (float)y, (float)z, 0.f, 0.f, (float)normal_direction, 0.f+uv_x, 0.f+uv_y, 0.f+uv_ov_x, 0.f+uv_ov_y, tint.r, tint.g, tint.b,
        (float)x+1.f, (float)y, (float)z, 0.f, 0.f, (float)normal_direction, TEXTURE_TILE_WIDTH+uv_x, 0.f+uv_y, TEXTURE_TILE_WIDTH+uv_ov_x, 0.f+uv_ov_y, tint.r, tint.g, tint.b,
        (float)x+1.f, (float)y+1.f, (float)z, 0.f, 0.f, (float)normal_direction, TEXTURE_TILE_WIDTH+uv_x, TEXTURE_TILE_HEIGHT+uv_y, TEXTURE_TILE_WIDTH+uv_ov_x, TEXTURE_TILE_HEIGHT+uv_ov_y, tint.r, tint.g, tint.b,
        (float)x, (float)y+1.f, (float)z, 0.f, 0.f, (float)normal_direction, 0.f+uv_x, TEXTURE_TILE_HEIGHT+uv_y, 0.f+uv_ov_x, TEXTURE_TILE_HEIGHT+uv_ov_y, tint.r, tint.g, tint.b
    };
    // clang-format on

    vertices->reserve(vertices->size() + distance(new_vertices.begin(), new_vertices.end()));
    vertices->insert(vertices->end(), new_vertices.begin(), new_vertices.end());

    push_indices(indices, offset, -normal_direction);
}

std::pair<std::pair<float, float>, std::pair<float, float>> get_uv_offset(BlockId_t block_id, uint8_t face)
{
    const std::array<std::pair<std::pair<int, int>, std::pair<int, int>>, 6> uv_offsets = blocks_uvs[block_id];
    return std::make_pair(std::make_pair(16.f * uv_offsets[face].first.first / TEXTURE_BLOCKS_WIDTH, 16.f * uv_offsets[face].first.second / TEXTURE_BLOCKS_HEIGHT), std::make_pair(16.f * uv_offsets[face].second.first / TEXTURE_BLOCKS_WIDTH, 16.f * uv_offsets[face].second.second / TEXTURE_BLOCKS_HEIGHT));
}

bool contains_and_not(std::vector<int> *vec, int elem, int dis)
{
    if (elem == dis)
        return false;
    for (size_t i = 0; i < vec->size(); i++)
    {
        if ((*vec)[i] == elem)
            return true;
    }
    return false;
}

void generate_slice_mesh(World_t *world, Slice_t *slice, Chunk_t *chunk)
{
    Section_t *section = &world->section;
    float slice_x = chunk->x;
    float slice_y = chunk->y;
    float slice_z = slice->z;
    std::vector<float> *vertices = &slice->mesh_blocks.vertices;
    std::vector<unsigned int> *indices = &slice->mesh_blocks.indices;
    for (size_t x = 0; x < 16; x++)
    {
        for (size_t y = 0; y < 16; y++)
        {
            for (size_t z = 0; z < 16; z++)
            {
                Block_t *current_block = get_block(section, chunk, slice, x, y, z);
                BlockId_t current_block_id = current_block->block_id;
                bool g_top;
                bool g_bottom;
                bool g_left;
                bool g_right;
                bool g_front;
                bool g_back;
                switch (current_block_id)
                {
                case 0:
                    // AIR
                    continue;
                    break;
                case 6:
                    vertices = &slice->mesh_foliage.vertices;
                    indices = &slice->mesh_foliage.indices;
                    {
                        bool next_to_air = get_block(section, chunk, slice, x, y, z - 1)->block_id == 0 |
                                           get_block(section, chunk, slice, x, y, z + 1)->block_id == 0 |
                                           get_block(section, chunk, slice, x - 1, y, z)->block_id == 0 |
                                           get_block(section, chunk, slice, x + 1, y, z)->block_id == 0 |
                                           get_block(section, chunk, slice, x, y - 1, z)->block_id == 0 |
                                           get_block(section, chunk, slice, x, y + 1, z)->block_id == 0;
                        g_top = next_to_air;
                        g_bottom = next_to_air;
                        g_left = next_to_air;
                        g_right = next_to_air;
                        g_front = next_to_air;
                        g_back = next_to_air;
                        break;
                    }
                default:
                    vertices = &slice->mesh_blocks.vertices;
                    indices = &slice->mesh_blocks.indices;
                    std::vector<int> allowed_ids = {0, 6};
                    g_top = contains_and_not(&allowed_ids, get_block(section, chunk, slice, x, y, z + 1)->block_id, current_block_id);
                    g_bottom = contains_and_not(&allowed_ids, get_block(section, chunk, slice, x, y, z - 1)->block_id, current_block_id);
                    g_left = contains_and_not(&allowed_ids, get_block(section, chunk, slice, x - 1, y, z)->block_id, current_block_id);
                    g_right = contains_and_not(&allowed_ids, get_block(section, chunk, slice, x + 1, y, z)->block_id, current_block_id);
                    g_front = contains_and_not(&allowed_ids, get_block(section, chunk, slice, x, y - 1, z)->block_id, current_block_id);
                    g_back = contains_and_not(&allowed_ids, get_block(section, chunk, slice, x, y + 1, z)->block_id, current_block_id);
                    break;
                }

                if (g_left)
                {
                    auto [uv_offset, uv_overlay_offset] = get_uv_offset(current_block_id, 2);
                    auto [uv_offset_x, uv_offset_y] = uv_offset;
                    auto [uv_overlay_offset_x, uv_overlay_offset_y] = uv_overlay_offset;
                    add_face_x(vertices, indices, x + slice_x, y + slice_y, z + slice_z, slice_z, -1, uv_offset_x, uv_offset_y, uv_overlay_offset_x, uv_overlay_offset_y, current_block->tint);
                }
                if (g_right)
                {
                    auto [uv_offset, uv_overlay_offset] = get_uv_offset(current_block_id, 4);
                    auto [uv_offset_x, uv_offset_y] = uv_offset;
                    auto [uv_overlay_offset_x, uv_overlay_offset_y] = uv_overlay_offset;
                    add_face_x(vertices, indices, x + slice_x + 1, y + slice_y, z + slice_z, slice_z, 1, uv_offset_x, uv_offset_y, uv_overlay_offset_x, uv_overlay_offset_y, current_block->tint);
                }
                if (g_front)
                {
                    auto [uv_offset, uv_overlay_offset] = get_uv_offset(current_block_id, 1);
                    auto [uv_offset_x, uv_offset_y] = uv_offset;
                    auto [uv_overlay_offset_x, uv_overlay_offset_y] = uv_overlay_offset;
                    add_face_y(vertices, indices, x + slice_x, y + slice_y, z + slice_z, slice_z, -1, uv_offset_x, uv_offset_y, uv_overlay_offset_x, uv_overlay_offset_y, current_block->tint);
                }
                if (g_back)
                {
                    auto [uv_offset, uv_overlay_offset] = get_uv_offset(current_block_id, 3);
                    auto [uv_offset_x, uv_offset_y] = uv_offset;
                    auto [uv_overlay_offset_x, uv_overlay_offset_y] = uv_overlay_offset;
                    add_face_y(vertices, indices, x + slice_x, y + slice_y + 1, z + slice_z, slice_z, 1, uv_offset_x, uv_offset_y, uv_overlay_offset_x, uv_overlay_offset_y, current_block->tint);
                }
                if (g_bottom)
                {
                    auto [uv_offset, uv_overlay_offset] = get_uv_offset(current_block_id, 5);
                    auto [uv_offset_x, uv_offset_y] = uv_offset;
                    auto [uv_overlay_offset_x, uv_overlay_offset_y] = uv_overlay_offset;
                    add_face_z(vertices, indices, x + slice_x, y + slice_y, z + slice_z, slice_z, -1, uv_offset_x, uv_offset_y, uv_overlay_offset_x, uv_overlay_offset_y, current_block->tint);
                }
                if (g_top)
                {
                    auto [uv_offset, uv_overlay_offset] = get_uv_offset(current_block_id, 0);
                    auto [uv_offset_x, uv_offset_y] = uv_offset;
                    auto [uv_overlay_offset_x, uv_overlay_offset_y] = uv_overlay_offset;
                    add_face_z(vertices, indices, x + slice_x, y + slice_y, z + slice_z + 1, slice_z, 1, uv_offset_x, uv_offset_y, uv_overlay_offset_x, uv_overlay_offset_y, current_block->tint);
                }
            }
        }
    }
}

static mContext_t C;

void buildUi()
{
    ImGui::Begin("stats");
    ImGui::Text("FPS %i", C.fps);
    ImGui::Text("dt %dms", C.dt);
    ImGui::Text("draw count %i", C.dc);
    ImGui::SliderFloat("SSAO strength", &C.debug.ssao_strength, 0.f, 1.f);
    ImGui::SliderInt("Target fps", (int *)(&C.target_fps), 0, 240);
    ImGui::Text("position: %f, %f, %f", C.world->main_camera->position.x, C.world->main_camera->position.y, C.world->main_camera->position.z);
    ImGui::End();
}

char *readfile(const char *filepath)
{
    FILE *fp;
    fopen_s(&fp, filepath, "r");
    if (!fp)
    {
        printf("[ERROR] Failed to open %s", filepath);
        return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    long lSize = ftell(fp);
    rewind(fp);
    char *buffer = (char *)calloc(1, lSize + 1);
    if (!buffer)
    {
        printf("[ERROR] Failed to allocate memory for file %s", filepath);
        return NULL;
    }
    fread(buffer, lSize, 1, fp);
    return buffer;
}

void create_shader(const char *vertex_path, const char *fragment_path, unsigned int *program)
{
    *program = glCreateProgram();
    char *vertex_shader_source = readfile(vertex_path);
    unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);
    free(vertex_shader_source);

    char *fragment_shader_source = readfile(fragment_path);
    unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);
    free(fragment_shader_source);

    glAttachShader(*program, vertex_shader);
    glAttachShader(*program, fragment_shader);
    glLinkProgram(*program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
}

void update_transform(Transform_t *transform)
{
    if (!transform->dirty)
        return;
    transform->transform = glm::translate(glm::mat4(1.f), transform->position) *
                           glm::rotate(glm::mat4(1.f), transform->rotation.b, glm::vec3(0, 0, 1)) *
                           glm::rotate(glm::mat4(1.f), transform->rotation.r, glm::vec3(1, 0, 0)) *
                           glm::rotate(glm::mat4(1.f), transform->rotation.g, glm::vec3(0, 0, 1)) *
                           glm::scale(glm::mat4(1.f), transform->scale);
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    ImGuiIO &io = ImGui::GetIO();
    io.AddMousePosEvent(xpos, ypos);
    if (C.input.mouse_capture)
    {
        float xoffset = xpos - C.input.last_x;
        float yoffset = C.input.last_y - ypos;
        C.input.last_x = xpos;
        C.input.last_y = ypos;

        float sensitivity = 0.1f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        C.world->main_camera->yaw += xoffset;
        C.world->main_camera->pitch += yoffset;

        if (C.world->main_camera->pitch > 89.0f)
            C.world->main_camera->pitch = 89.0f;
        if (C.world->main_camera->pitch < -89.0f)
            C.world->main_camera->pitch = -89.0f;
    }
}

void set_capture_cursor(GLFWwindow *window, bool capture)
{
    if (capture)
    {
        C.input.mouse_capture = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetCursorPos(window, C.input.last_x, C.input.last_y);
    }
    else
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        C.input.mouse_capture = false;
    }
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    ImGuiIO &io = ImGui::GetIO();
    io.AddMouseButtonEvent(button, action == GLFW_PRESS);
    if (!C.input.mouse_capture && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && !io.WantCaptureMouse)
    {
        set_capture_cursor(window, true);
    }
}

void mousewheel_callback(GLFWwindow *window, double wheel_x, double wheel_y)
{
    ImGuiIO &io = ImGui::GetIO();
    io.AddMouseWheelEvent(wheel_x, wheel_y);
    if (!io.WantCaptureMouse)
    {
        if (wheel_y < 0)
        {
            C.world->main_camera->freeflight_speed /= 2;
        }
        else
        {
            C.world->main_camera->freeflight_speed *= 2;
        }
    }
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    ImGuiIO &io = ImGui::GetIO();
    static float fov_backup = 0.f;
    if (!io.WantCaptureKeyboard)
    {
        if (C.input.mouse_capture)
        {
            Camera_t *camera = C.world->main_camera;
            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            {
                set_capture_cursor(window, false);
            }
            if (key == GLFW_KEY_R && action == GLFW_PRESS)
            {
                camera->position = glm::vec3(0.f);
                camera->pitch = 0.f;
                camera->yaw = 0.f;
            }
            if (key == GLFW_KEY_C && action == GLFW_PRESS)
            {
                fov_backup = camera->fov;
                camera->fov = camera->fov / 4;
            }
            if (key == GLFW_KEY_C && action == GLFW_RELEASE)
            {
                camera->fov = fov_backup;
            }
            if (key == GLFW_KEY_M && action == GLFW_RELEASE)
            {
                switch (C.world->main_camera->mode)
                {
                case CameraMode::Player:
                    C.world->main_camera->mode = CameraMode::Freeflight;
                    break;
                case CameraMode::Freeflight:
                default:
                    C.world->player->position = C.world->main_camera->position - glm::vec3(0.f, 0.f, 1.4f);
                    C.world->player->last_position = C.world->main_camera->position - glm::vec3(0.f, 0.f, 1.4f);
                    C.world->main_camera->mode = CameraMode::Player;
                    break;
                }
            }
        }
    }
}

void update_player(GLFWwindow *window)
{
    Camera_t *camera = C.world->main_camera;
    Player_t *player = C.world->player;
    player->acceleration = glm::vec3();
    apply_gravity(player);
    double dt = C.dt;
    glm::vec3 offset = glm::vec3(0.f);
    switch (camera->mode)
    {
    case CameraMode::Freeflight:
    {
        glm::vec3 forward = camera->direction;
        glm::vec3 right = glm::normalize(glm::cross(camera->direction, camera->up));
        float speed = dt * camera->freeflight_speed;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        {
            offset += camera->direction;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        {
            offset += -camera->direction;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        {
            offset += -right;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        {
            offset += right;
        }
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        {
            offset += glm::vec3(0, 0, 1);
        }
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        {
            offset += -glm::vec3(0, 0, 1);
        }
        if (glm::length(offset) > 0.001f)
        {
            camera->position += speed * glm::normalize(offset);
        }
    }
    break;
    case CameraMode::Player:
        glm::vec3 forward = glm::normalize(glm::vec3(camera->direction.x, camera->direction.y, 0.));
        glm::vec3 right = glm::normalize(glm::cross(camera->direction, glm::vec3(0, 0, 1)));
        float speed = dt * 1000.f;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        {
            offset += forward;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        {
            offset += -forward;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        {
            offset += -right;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        {
            offset += right;
        }
        if (glm::length(offset) > 0.001f)
        {
            player->acceleration = speed * glm::normalize(offset);
        }
        camera->position = C.world->player->position + glm::vec3(0.f, 0.f, 1.4f);
        break;
    }
    update_verlet(player, C.dt);
    solve_collision(player, C.world);
}

void init_world(World_t *world)
{
    world->section.x = 0;
    world->section.y = 0;
    float freqs[] = {0.01f, 0.06f};
    float offsets[] = {30.f, 0.f};
    float ampls[] = {8.f, 1.f};
    init_perlin(&world->heightmap, freqs, offsets, ampls);
    for (size_t chunk_id = 0; chunk_id < sizeof(world->section.chunks) / sizeof(Chunk_t *); chunk_id++)
    {
        world->section.chunks[chunk_id] = nullptr;
    }
}

void free_world(World_t *world)
{
    free_perlin(&world->heightmap);
}

void record_framebuffer(unsigned int *gBuffer, unsigned int *gPosition, unsigned int *gNormal, unsigned int *gColor, uint32_t width, uint32_t height)
{
    if (*gBuffer != 0)
        glDeleteFramebuffers(1, gBuffer);
    if (*gPosition != 0)
        glDeleteTextures(1, gPosition);
    if (*gNormal != 0)
        glDeleteTextures(1, gNormal);
    if (*gColor != 0)
        glDeleteTextures(1, gColor);

    glGenFramebuffers(1, gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, *gBuffer);

    // - position color buffer
    glGenTextures(1, gPosition);
    glBindTexture(GL_TEXTURE_2D, *gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *gPosition, 0);
    // normal color buffer
    glGenTextures(1, gNormal);
    glBindTexture(GL_TEXTURE_2D, *gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, *gNormal, 0);
    // color + specular color buffer
    glGenTextures(1, gColor);
    glBindTexture(GL_TEXTURE_2D, *gColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, *gColor, 0);
    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
    unsigned int attachments[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, attachments);

    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

int main(int argc, char **argv)
{
    if (!glfwInit())
    {
        std::cout << "Failed to initialize GLFW" << std::endl;
    }

    static int screen_width = 1280;
    static int screen_height = 720;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(screen_width, screen_height, "Minecraft mais beaucoup moins bien et avec plein de bugs", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    gladLoadGL();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    ImGui_ImplGlfw_InitForOpenGL(window, true); // Second param install_callback=true will install GLFW callbacks and chain to existing ones.
    ImGui_ImplOpenGL3_Init();

    double last_time = glfwGetTime();

    // Shader creation
    unsigned int cube_shader_program;
    create_shader("resources/blocks.vert", "resources/blocks.frag", &cube_shader_program);

    unsigned int deferred_shader_program;
    create_shader("resources/deferred.vert", "resources/deferred.frag", &deferred_shader_program);

    // Texture
    int texture_width, texture_height, texture_depth;
    unsigned char *texture_data = stbi_load("resources/blocks.png", &texture_width, &texture_height, &texture_depth, 0);
    if (!texture_data)
    {
        printf("Failed to load blocks.png\n");
        exit(EXIT_FAILURE);
    }
    printf("Loaded blocks.png (%i, %i, %i)\n", texture_width, texture_height, texture_depth);

    unsigned int blocks_texture;
    glGenTextures(1, &blocks_texture);
    glBindTexture(GL_TEXTURE_2D, blocks_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_width, texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(texture_data);

    // Shader select
    glUseProgram(cube_shader_program);

    World_t world;
    init_world(&world);
    C.world = &world;

    Camera_t camera = {};
    camera.fov = 60.f / 180.f * glm::pi<float>();
    camera.near = 0.1;
    camera.far = 1000.;
    camera.position = glm::vec3(-10.f, -10.f, 10.f);
    C.world->main_camera = &camera;

    Player_t player;
    player.position = glm::vec3(30., 30., 60.);
    player.last_position = glm::vec3(30., 30., 60.0);
    world.player = &player;
    // camera.mode = CameraMode::Player;
    camera.mode = CameraMode::Freeflight;

    // Uniforms
    int modelLoc = glGetUniformLocation(cube_shader_program, "view_projection");

    for (int i = 0; i < 10; i++)
    {
        for (int j = 0; j < 10; j++)
        {
            // Test chunk removal
            // if (i==4 && j==3) continue;
            Chunk_t *chunk = new Chunk_t;
            init_chunk(chunk, &world.section, j, i);
            generate_chunk(&world, chunk);
            world.section.chunks[chunk_id(j, i)] = chunk;
        }
    }
    for (size_t i = 0; i < 40; i++)
    {
        const int64_t x = (float)rand() / (float)RAND_MAX * 10 * 16;
        const int64_t y = (float)rand() / (float)RAND_MAX * 10 * 16;
        spawn_tree(&world, glm::vec3(x, y, sample_perlin(&world.heightmap, x, y, 0) + 1), 4);
    }

    for (int i = 0; i < 16; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            Chunk_t *chunk = world.section.chunks[chunk_id(j, i)];
            if (chunk == NULL)
                continue;
            for (size_t slice_index = 0; slice_index < 24; slice_index++)
            {
                Slice_t *slice = &chunk->slices[slice_index];
                generate_slice_mesh(&world, slice, chunk);
                // VAO
                glGenVertexArrays(1, &slice->mesh_blocks.vao);
                glGenBuffers(1, &slice->mesh_blocks.vbo);
                glGenBuffers(1, &slice->mesh_blocks.ebo);

                glBindVertexArray(slice->mesh_blocks.vao);

                // VBO
                glBindBuffer(GL_ARRAY_BUFFER, slice->mesh_blocks.vbo);
                glBufferData(GL_ARRAY_BUFFER, slice->mesh_blocks.vertices.size() * sizeof(float), slice->mesh_blocks.vertices.data(), GL_STATIC_DRAW);
                // EBO
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, slice->mesh_blocks.ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, slice->mesh_blocks.indices.size() * sizeof(unsigned int), slice->mesh_blocks.indices.data(), GL_STATIC_DRAW);

                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 13 * sizeof(float), (void *)0);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 13 * sizeof(float), (void *)(3 * sizeof(float)));
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 13 * sizeof(float), (void *)(6 * sizeof(float)));
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 13 * sizeof(float), (void *)(8 * sizeof(float)));
                glEnableVertexAttribArray(3);
                glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 13 * sizeof(float), (void *)(10 * sizeof(float)));
                glEnableVertexAttribArray(4);

                // VAO
                glGenVertexArrays(1, &slice->mesh_foliage.vao);
                glGenBuffers(1, &slice->mesh_foliage.vbo);
                glGenBuffers(1, &slice->mesh_foliage.ebo);

                glBindVertexArray(slice->mesh_foliage.vao);

                // VBO
                glBindBuffer(GL_ARRAY_BUFFER, slice->mesh_foliage.vbo);
                glBufferData(GL_ARRAY_BUFFER, slice->mesh_foliage.vertices.size() * sizeof(float), slice->mesh_foliage.vertices.data(), GL_STATIC_DRAW);
                // EBO
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, slice->mesh_foliage.ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, slice->mesh_foliage.indices.size() * sizeof(unsigned int), slice->mesh_foliage.indices.data(), GL_STATIC_DRAW);

                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 13 * sizeof(float), (void *)0);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 13 * sizeof(float), (void *)(3 * sizeof(float)));
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 13 * sizeof(float), (void *)(6 * sizeof(float)));
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 13 * sizeof(float), (void *)(8 * sizeof(float)));
                glEnableVertexAttribArray(3);
                glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 13 * sizeof(float), (void *)(10 * sizeof(float)));
                glEnableVertexAttribArray(4);
            }
        }
    }

    unsigned int gBuffer, gPosition, gNormal, gColor = 0;

    record_framebuffer(&gBuffer, &gPosition, &gNormal, &gColor, screen_width, screen_height);

    unsigned int fullscreen_vao, fullscreen_vbo, fullscreen_ebo;
    glGenVertexArrays(1, &fullscreen_vao);
    glGenBuffers(1, &fullscreen_vbo);
    glGenBuffers(1, &fullscreen_ebo);
    glBindVertexArray(fullscreen_vao);
    // VBO
    glBindBuffer(GL_ARRAY_BUFFER, fullscreen_vbo);
    glBufferData(GL_ARRAY_BUFFER, fullscreen_rect_vertices.size() * sizeof(float), fullscreen_rect_vertices.data(), GL_STATIC_DRAW);
    // EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fullscreen_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, fullscreen_rect_indices.size() * sizeof(unsigned int), fullscreen_rect_indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    // SSAO
    std::vector<glm::vec3> ssaoKernel;
    for (unsigned int i = 0; i < 64; ++i)
    {
        glm::vec3 sample(
            (float)rand() / (float)RAND_MAX * 2.0 - 1.0,
            (float)rand() / (float)RAND_MAX * 2.0 - 1.0,
            (float)rand() / (float)RAND_MAX);
        sample = glm::normalize(sample);
        sample *= (float)rand() / (float)RAND_MAX;
        float scale = (float)i / 64.0;
        scale = 0.1f + (scale * scale) * 0.9f;
        sample *= scale;
        ssaoKernel.push_back(sample);
    }
    int samples_loc = glGetUniformLocation(deferred_shader_program, "hemisphere_samples");

    std::vector<glm::vec3> ssaoNoise;
    for (unsigned int i = 0; i < 16; i++)
    {
        glm::vec3 noise(
            (float)rand() / (float)RAND_MAX * 2.0 - 1.0,
            (float)rand() / (float)RAND_MAX * 2.0 - 1.0,
            0.0f);
        ssaoNoise.push_back(noise);
    }

    unsigned int noiseTexture;
    glGenTextures(1, &noiseTexture);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, mousewheel_callback);
    glfwSetKeyCallback(window, key_callback);

    double last_frame_time = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        C.dc = 0;
        double current_time = glfwGetTime();
        double elapsed_time = current_time - last_time;
        if (C.target_fps > 0 && elapsed_time < 1.f / (float)C.target_fps)
        {
            continue;
        }
        C.fps = int(1. / elapsed_time);
        C.dt = elapsed_time;

        update_player(window);
        Camera_t *camera = C.world->main_camera;
        camera->direction = {cos(glm::radians(camera->yaw)) * cos(glm::radians(camera->pitch)), -sin(glm::radians(camera->yaw)) * cos(glm::radians(camera->pitch)), sin(glm::radians(camera->pitch))};
        glm::mat4 view = glm::lookAt(camera->position, camera->position + camera->direction, camera->up);
        glm::mat4 projection = glm::perspective(camera->fov, (float)screen_width / (float)screen_height, camera->near, camera->far);
        glm::mat4 VP = projection * view;

        int new_width, new_height;
        glfwGetFramebufferSize(window, &new_width, &new_height);
        if (new_width != screen_width || new_height != screen_height)
        {
            record_framebuffer(&gBuffer, &gPosition, &gNormal, &gColor, new_width, new_height);
        }
        screen_width = new_width;
        screen_height = new_height;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        glClearColor(0.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render to g-buffer
        glEnable(GL_DEPTH_TEST);

        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, screen_width, screen_height);

        glUseProgram(cube_shader_program);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(VP));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, blocks_texture);

        for (size_t i = 0; i < sizeof(world.section.chunks) / sizeof(Chunk_t *); i++)
        {
            if (world.section.chunks[i] == nullptr)
                continue;
            Chunk_t *chunk = world.section.chunks[i];
            for (size_t slice_index = 0; slice_index < 24; slice_index++)
            {
                Slice_t *slice = &chunk->slices[slice_index];
                if (slice == nullptr)
                    continue;
                size_t count = slice->mesh_blocks.indices.size();
                if (count != 0)
                {
                    glBindVertexArray(slice->mesh_blocks.vao);

                    glEnable(GL_CULL_FACE);
                    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0);
                    C.dc++;
                }

                count = slice->mesh_foliage.indices.size();
                if (count != 0)
                {
                    glBindVertexArray(slice->mesh_foliage.vao);

                    // glDisable(GL_CULL_FACE);
                    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0);
                    C.dc++;
                }
            }
        }

        // Deferred
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, screen_width, screen_height);

        glUseProgram(deferred_shader_program);
        glUniformMatrix4fv(glGetUniformLocation(deferred_shader_program, "projection"), 1, GL_FALSE, glm::value_ptr(VP));
        glUniform1fv(glGetUniformLocation(deferred_shader_program, "ssao_strength"), 1, &C.debug.ssao_strength);
        for (unsigned int i = 0; i < 64; ++i)
            glUniform3fv(glGetUniformLocation(deferred_shader_program, ("hemisphere_samples[" + std::to_string(i) + "]").c_str()), 1, &ssaoKernel[i][0]);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gPosition);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gNormal);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gColor);
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, noiseTexture);

        glBindVertexArray(fullscreen_vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        buildUi();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        glfwPollEvents();
        last_time = current_time;
    }
    for (size_t i = 0; i < sizeof(world.section.chunks) / sizeof(Chunk_t *); i++)
    {
        if (world.section.chunks[i] != nullptr)
        {
            free(world.section.chunks[i]);
        }
    }
    free_world(&world);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
