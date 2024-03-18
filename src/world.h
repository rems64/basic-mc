#ifndef WORLD_H
#define WORLD_H

#include <cstdint>
#include "types.h"

constexpr size_t block_index(uint8_t x, uint8_t y, uint8_t z)
{
    return x + 16 * y + 256 * z;
}

constexpr size_t chunk_id(uint32_t x, uint32_t y)
{
    return x + 16 * y;
}

uint16_t *get_global_block(World_t *world, int64_t x, int64_t y, int64_t z)
{
    if (x < 0 || y < 0 || x > 256 || y > 256 || z < 0 || z > 16 * 24)
        return NULL;
    Section_t *section = &world->section;
    int64_t chunk_x = x / 16;
    int64_t chunk_y = y / 16;
    Chunk_t *chunk = section->chunks[chunk_id(x / 16, y / 16)];
    if (chunk==NULL)
        return NULL;
    Slice_t *slice = &chunk->slices[z / 16];
    if (slice==NULL)
        return NULL;
    uint8_t block_x = x - chunk->x;
    uint8_t block_y = y - chunk->y;
    uint8_t block_z = z - slice->z;
    return &slice->blocks[block_index(block_x, block_y, block_z)];
}

Block_t *get_world_block(World_t *world, int64_t x, int64_t y, int64_t z)
{
    if (x < 0 || y < 0 || x > 256 || y > 256 || z < 0 || z > 16 * 24)
        return NULL;
    Section_t *section = &world->section;
    if (section==NULL)
        return NULL;
    int64_t chunk_x = x / 16;
    int64_t chunk_y = y / 16;
    Chunk_t *chunk = section->chunks[chunk_id(x / 16, y / 16)];
    if (chunk==NULL)
        return NULL;
    Slice_t *slice = &chunk->slices[z / 16];
    if (slice==NULL)
        return NULL;
    uint8_t block_x = x - chunk->x;
    uint8_t block_y = y - chunk->y;
    uint8_t block_z = z - slice->z;
    return &slice->table[slice->blocks[block_index(block_x, block_y, block_z)]];
}

#endif