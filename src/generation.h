#ifndef GENERATION_H
#define GENERATION_H

#include <cstdlib>
#include <cstring>

#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

typedef struct Perlin
{
    size_t octaves_count;
    float *octaves_frequencies;
    float *octaves_offsets;
    float *octaves_amplitudes;
} Perlin_t;

void init_perlin(Perlin_t *perlin, float octaves_frequencies[], float octaves_offsets[], float octaves_amplitudes[])
{
    size_t count = sizeof(octaves_frequencies) / sizeof(float);
    perlin->octaves_count = count;
    perlin->octaves_frequencies = (float *)malloc(count * sizeof(float));
    perlin->octaves_offsets = (float *)malloc(count * sizeof(float));
    perlin->octaves_amplitudes = (float *)malloc(count * sizeof(float));

    std::memcpy(perlin->octaves_frequencies, octaves_frequencies, sizeof(octaves_frequencies));
    std::memcpy(perlin->octaves_offsets, octaves_offsets, sizeof(octaves_offsets));
    std::memcpy(perlin->octaves_amplitudes, octaves_amplitudes, sizeof(octaves_amplitudes));
}

double sample_perlin(Perlin_t *perlin, double x, double y, double z)
{
    double val = 0.;
    for (size_t octave_index = 0; octave_index < perlin->octaves_count; octave_index++)
    {
        float freq = perlin->octaves_frequencies[octave_index];
        float ampl = perlin->octaves_amplitudes[octave_index];
        float offset = perlin->octaves_offsets[octave_index];

        val += ampl * stb_perlin_noise3(freq * x, freq * y, freq * z, 0, 0, 0) + offset;
    }

    return val;
}

void free_perlin(Perlin_t *perlin)
{
    free(perlin->octaves_frequencies);
    free(perlin->octaves_amplitudes);
}

#endif