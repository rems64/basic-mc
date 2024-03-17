#version 460 core

out vec4 outColor;
in vec2 TexCoords;

layout(binding=0) uniform sampler2D gPosition;
layout(binding=1) uniform sampler2D gNormal;
layout(binding=2) uniform sampler2D gAlbedo;

void main()
{
    // retrieve data from G-buffer
    vec3 position = texture(gPosition, TexCoords).rgb;
    vec3 normal = texture(gNormal, TexCoords).rgb;
    vec3 albedo = texture(gAlbedo, TexCoords).rgb;

    vec3 sunDir = normalize(vec3(1.0, 0.0, -1.0));
    float ambiant = 0.4f;
    float luminosity = min(1.f, ambiant+max(0.f, -dot(sunDir, normal)));
    
    outColor = vec4(luminosity*albedo, 1.0);
    // FragColor = vec4(TexCoords, 0.0, 1.0);
}  