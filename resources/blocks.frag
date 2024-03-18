#version 330 core

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedo;

in vec3 Pos;
in vec3 Normal;
in vec2 UV;
in vec2 UVOverlay;
in vec3 Tint;

uniform sampler2D blocksTexture;
void main()
{
   vec4 color = texture(blocksTexture, UV);
   vec4 overlay = texture(blocksTexture, UVOverlay);

   gPosition = Pos;
   gNormal = normalize(Normal);
   vec4 res = vec4(Tint, 1.0)*overlay.a*overlay+(1.0-overlay.a)*color; 
   if (res.a<0.5) {
      discard;
   }
   gAlbedo = res;
}