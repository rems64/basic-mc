#ifndef NOISE_H
#define NOISE_H

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct Perlin2d
{
    std::vector<std::vector<glm::vec2>> grid;
};

float random_float()
{
    float r = rand() / RAND_MAX;
    return 2. * r - 1.;
}

void generate_perlin2d(Perlin2d *perlin, uint32_t width, uint32_t height)
{
    for (uint32_t i = 0; i < height; i++)
    {
        perlin->grid.push_back(std::vector<glm::vec2>{});
        for (uint32_t j = 0; j < width; j++)
        {
            std::vector<glm::vec2> *vec = &perlin->grid[perlin->grid.size() - 1];
            vec->push_back(glm::normalize(glm::vec2(random_float(), random_float())));
        }
    }
}

glm::vec2 randomGradient(int ix, int iy)
{
    // No precomputed gradients mean this works for any number of grid coordinates
    const unsigned w = 8 * sizeof(unsigned);
    const unsigned s = w / 2; // rotation width
    unsigned a = ix, b = iy;
    a *= 3284157443;
    b ^= a << s | a >> w - s;
    b *= 1911520717;
    a ^= b << s | b >> w - s;
    a *= 2048419325;
    float random = a * (3.14159265 / ~(~0u >> 1)); // in [0, 2*Pi]
    glm::vec2 v;
    v.x = cos(random);
    v.y = sin(random);
    return v;
}

// Tmp solution from wikipedia
float interpolate(float a0, float a1, float w)
{
    if (0.0 > w)
        return a0;
    if (1.0 < w)
        return a1;
    return (a1 - a0) * w + a0;
    /* // Use this cubic interpolation [[Smoothstep]] instead, for a smooth appearance:
     * return (a1 - a0) * (3.0 - w * 2.0) * w * w + a0;
     *
     * // Use [[Smootherstep]] for an even smoother result with a second derivative equal to zero on boundaries:
     * return (a1 - a0) * ((w * (w * 6.0 - 15.0) + 10.0) * w * w * w) + a0;
     */
}

// Computes the dot product of the distance and gradient vectors.
float dotGridGradient(int ix, int iy, float x, float y)
{
    // Get gradient from integer coordinates
    glm::vec2 gradient = randomGradient(ix, iy);

    // Compute the distance vector
    float dx = x - (float)ix;
    float dy = y - (float)iy;

    // Compute the dot-product
    return (dx * gradient.x + dy * gradient.y);
}

// Compute Perlin noise at coordinates x, y
float sample_perlin2d(float x, float y)
{
    // Determine grid cell coordinates
    int x0 = (int)floor(x);
    int x1 = x0 + 1;
    int y0 = (int)floor(y);
    int y1 = y0 + 1;

    // Determine interpolation weights
    // Could also use higher order polynomial/s-curve here
    float sx = x - (float)x0;
    float sy = y - (float)y0;

    // Interpolate between grid point gradients
    float n0, n1, ix0, ix1, value;

    n0 = dotGridGradient(x0, y0, x, y);
    n1 = dotGridGradient(x1, y0, x, y);
    ix0 = interpolate(n0, n1, sx);

    n0 = dotGridGradient(x0, y1, x, y);
    n1 = dotGridGradient(x1, y1, x, y);
    ix1 = interpolate(n0, n1, sx);

    value = interpolate(ix0, ix1, sy);
    return 0.5 * value + 0.5;
}

#endif