#version 330 core

out vec4 FragColor;
in vec3 Normal;
in vec2 UV;
in vec2 UVOverlay;
in vec3 Tint;
uniform sampler2D blocksTexture;
void main()
{
   vec3 sunDir = normalize(vec3(1.0, 0.0, -1.0));
   float ambiant = 0.4f;
   float luminosity = min(1.f, ambiant+max(0.f, -dot(sunDir, Normal)));
   vec4 color = texture(blocksTexture, UV);
   vec4 overlay = texture(blocksTexture, UVOverlay);
   FragColor = luminosity*(vec4(Tint, 1.0)*overlay.a*overlay+(1.0-overlay.a)*color);
}