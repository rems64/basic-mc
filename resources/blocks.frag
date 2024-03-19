#version 330 core

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedo;

in VS_OUT {
   vec3 position;
   vec3 normal;
   vec2 uv_base;
   vec2 uv_overlay;
   vec3 tint;
} vs_in;

uniform sampler2D blocksTexture;
void main()
{
   vec4 color = texture(blocksTexture, vs_in.uv_base);
   vec4 overlay = texture(blocksTexture, vs_in.uv_overlay);

   gPosition = vs_in.position;
   gNormal = normalize(vs_in.normal);
   vec4 res = vec4(vs_in.tint, 1.0)*overlay.a*overlay+(1.0-overlay.a)*color; 
   if (res.a<0.5) {
      discard;
   }
   gAlbedo = res;
}