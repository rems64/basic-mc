#version 460 core

out vec4 outColor;
in vec2 TexCoords;

layout(binding=0) uniform sampler2D s_gPosition;
layout(binding=1) uniform sampler2D s_gNormal;
layout(binding=2) uniform sampler2D s_gAlbedo;
layout(binding=10) uniform sampler2D s_noise;

uniform vec3 hemisphere_samples[64];
uniform float ssao_strength;
uniform mat4 projection;

const vec2 noiseScale = vec2(800.0/4.0, 600.0/4.0);
const int kernelSize = 32;
const float radius = 2.0;
const float bias = 0.1;

void main()
{
    // retrieve data from G-buffer
    vec3 position = texture(s_gPosition, TexCoords).rgb;
    vec3 normal = texture(s_gNormal, TexCoords).rgb;
    vec3 albedo = texture(s_gAlbedo, TexCoords).rgb;
    vec3 noise = texture(s_noise, TexCoords * noiseScale).rgb;

    vec3 tangent   = normalize(noise - normal * dot(noise, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for(int i = 0; i < kernelSize; ++i)
    {
        // get sample position
        vec3 samplePos = TBN * hemisphere_samples[i]; // from tangent to view-space
        samplePos = position + samplePos * radius;
        vec4 offset = vec4(samplePos, 1.0);
        offset      = projection * offset;    // from view to clip-space
        offset.xyz /= offset.w;               // perspective divide
        offset.xyz  = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0
        float sampleDepth = texture(s_gPosition, offset.xy).z;
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(position.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }
    occlusion = 1.0 - (occlusion / kernelSize);
    occlusion = 1.-ssao_strength+ssao_strength*occlusion;

    vec3 sunDir = normalize(vec3(1.0, 0.0, -1.0));
    float ambiant = 0.4f;
    float luminosity = min(1.f, ambiant+max(0.f, -dot(sunDir, normal)));
    
    outColor = vec4(occlusion*luminosity*albedo, 1.0);
    // outColor = vec4(vec3(occlusion), 1.0);
    // FragColor = vec4(TexCoords, 0.0, 1.0);
}  