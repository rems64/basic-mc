#ifndef PLAYER_H
#define PLAYER_H

#include <glm/glm.hpp>
#include "types.h"
#include "world.h"

void apply_gravity(Player_t *player)
{
    player->acceleration.z -= 32.f;
}

void update_verlet(Player_t *player, float dt)
{
    glm::vec3 position = player->position;
    player->position = 2.f * player->position - player->last_position + player->acceleration * dt * dt;
    player->last_position = position;
}

void solve_collision(Player_t *player, World_t *world)
{
    // glm::vec3 block_position = glm::vec3(floor(player->position.x), floor(player->position.y), floor(player->position.z));
    // const int64_t chunk_x = block_position.x/16;
    // const int64_t chunk_y = block_position.y/16;
    // const Chunk_t *chunk = world->section.chunks[chunk_id(chunk_x, chunk_y)];
    // const uint8_t block_x = block_position.x-chunk->x;
    // const uint8_t block_y = block_position.y-chunk->y;
    // const uint8_t block_z = block_position.z;
    // const uint8_t section_id = floor((float)block_position.z/16.f);
    // world->section.chunks[block_index(block_x, block_y, block_z)]
    Block_t *block = get_world_block(world, player->position.x, player->position.y, player->position.z);
    if (block == NULL)
        return;
    if (block->block_id != 0)
    {
        float delta_z = ceil(player->position.z) - player->position.z;
        player->position += glm::vec3(0, 0, delta_z);
    }
}

#endif