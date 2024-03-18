#ifndef TYPES_H
#define TYPES_H

#include <vector>
#include <glm/glm.hpp>
#include "blocks.h"
#include "generation.h"

typedef struct Transform
{
    glm::vec3 position = {0, 0, 0};
    glm::vec3 scale = {1, 1, 1};
    glm::vec3 rotation = {0, 0, 0};

    glm::mat4 transform;
    bool dirty;
} Transform_t;

enum CameraMode
{
    Freeflight,
    Player
};

typedef struct Camera
{
    float fov;
    float near;
    float far;

    float freeflight_speed = 50.f;

    glm::vec3 position;
    glm::vec3 direction;
    float yaw;
    float pitch;
    glm::vec3 up = {0, 0, 1};

    CameraMode mode = Freeflight;
} Camera_t;

typedef struct mInput
{
    double last_x = -1;
    double last_y = -1;
    bool mouse_capture = false;
} mInput_t;

typedef struct mDebugContext
{
    float ssao_strength = 1.f;
} mDebugContext_t;

typedef struct Block
{
    BlockId_t block_id;
    glm::vec3 tint = {1., 1., 1.};
} Block_t;

static Block_t block_air = {BlockId_t{0}};

// Slice: 16x16x16
// Chunk: 16x16x384, 24 slices high

typedef struct RenderMesh
{
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    unsigned int vao;
    unsigned int vbo;
    unsigned int ebo;
} RenderMesh_t;

typedef struct Slice
{
    uint8_t z;
    uint8_t index;
    RenderMesh_t mesh_blocks;
    RenderMesh_t mesh_foliage;
    std::vector<Block_t> table;
    uint16_t blocks[4096];
} Slice_t;

typedef struct Chunk
{
    int64_t x;
    int64_t y;
    uint32_t section_x;
    uint32_t section_y;
    Slice_t slices[24];
} Chunk_t;

typedef struct Section
{
    int64_t x;
    int64_t y;
    Chunk_t *chunks[16 * 16];
} Section_t;

typedef struct Player
{
    glm::vec3 position;
    glm::vec3 last_position;
    glm::vec3 acceleration;
} Player_t;

typedef struct World
{
    Section_t section;
    Perlin_t heightmap;
    Camera_t *main_camera = nullptr;
    Player_t *player;
} World_t;

typedef struct mContext
{
    int fps = 0;
    double dt = 0.;
    uint32_t target_fps = 60;
    uint32_t dc = 0;
    mInput_t input;
    mDebugContext_t debug;
    World_t *world = nullptr;
} mContext_t;

#endif