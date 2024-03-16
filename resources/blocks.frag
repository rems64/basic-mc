#version 330 core

out vec4 FragColor;
in vec3 Normal;
in vec2 UV;
uniform sampler2D blocksTexture;
void main()
{
   vec3 sunDir = normalize(vec3(1.0, 0.0, -1.0));
   float ambiant = 0.4f;
   float luminosity = min(1.f, ambiant+max(0.f, -dot(sunDir, Normal)));
   FragColor = luminosity*texture(blocksTexture, UV);
}