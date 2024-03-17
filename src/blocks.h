#include <unordered_map>
#include <array>

typedef uint32_t BlockId_t;
#define BLOCKID_AIR \
    BlockId_t { 0 }


// (color, tint_mask)
std::unordered_map<BlockId_t, std::array<std::pair<std::pair<int, int>, std::pair<int, int>>, 6>> blocks_uvs{
    // Stone
    {1, {std::make_pair(std::make_pair(6, 26), std::make_pair(0, 0)), std::make_pair(std::make_pair(6, 26), std::make_pair(0, 0)), std::make_pair(std::make_pair(6, 26), std::make_pair(0, 0)), std::make_pair(std::make_pair(6, 26), std::make_pair(0, 0)), std::make_pair(std::make_pair(6, 26), std::make_pair(0, 0)), std::make_pair(std::make_pair(6, 26), std::make_pair(0, 0))}},
    // Dirt
    {2, {std::make_pair(std::make_pair(21, 13), std::make_pair(0, 0)), std::make_pair(std::make_pair(21, 13), std::make_pair(0, 0)), std::make_pair(std::make_pair(21, 13), std::make_pair(0, 0)), std::make_pair(std::make_pair(21, 13), std::make_pair(0, 0)), std::make_pair(std::make_pair(21, 13), std::make_pair(0, 0)), std::make_pair(std::make_pair(21, 13), std::make_pair(0, 0))}},
    // Grass
    {3, {std::make_pair(std::make_pair(0, 0), std::make_pair(25, 11)), std::make_pair(std::make_pair(25, 8), std::make_pair(25, 9)), std::make_pair(std::make_pair(25, 8), std::make_pair(25, 9)), std::make_pair(std::make_pair(25, 8), std::make_pair(25, 9)), std::make_pair(std::make_pair(25, 8), std::make_pair(25, 9)), std::make_pair(std::make_pair(21, 13), std::make_pair(0, 0))}},
    // Bedrock
    {4, {std::make_pair(std::make_pair(11, 0), std::make_pair(0, 0)), std::make_pair(std::make_pair(11, 0), std::make_pair(0, 0)), std::make_pair(std::make_pair(11, 0), std::make_pair(0, 0)), std::make_pair(std::make_pair(11, 0), std::make_pair(0, 0)), std::make_pair(std::make_pair(11, 0), std::make_pair(0, 0)), std::make_pair(std::make_pair(11, 0), std::make_pair(0, 0))}},
    // Oak log
    {5, {std::make_pair(std::make_pair(6, 18), std::make_pair(0, 0)), std::make_pair(std::make_pair(5, 18), std::make_pair(0, 0)), std::make_pair(std::make_pair(5, 18), std::make_pair(0, 0)), std::make_pair(std::make_pair(5, 18), std::make_pair(0, 0)), std::make_pair(std::make_pair(5, 18), std::make_pair(0, 0)), std::make_pair(std::make_pair(6, 18), std::make_pair(0, 0))}},
    // Oak leaves
    {6, {std::make_pair(std::make_pair(0, 0), std::make_pair(4, 18)), std::make_pair(std::make_pair(0, 0), std::make_pair(4, 18)), std::make_pair(std::make_pair(0, 0), std::make_pair(4, 18)), std::make_pair(std::make_pair(0, 0), std::make_pair(4, 18)), std::make_pair(std::make_pair(0, 0), std::make_pair(4, 18)), std::make_pair(std::make_pair(0, 0), std::make_pair(4, 18))}}
};