#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec2 aUVOoverlay;
layout (location = 4) in vec3 aTint;

uniform mat4 view_projection;

out VS_OUT {
   vec3 position;
   vec3 normal;
   vec2 uv_base;
   vec2 uv_overlay;
   vec3 tint;
} vs_out;

void main()
{
   gl_Position = view_projection * vec4(aPos, 1.0);
   vs_out.position = aPos;
   vs_out.normal = aNormal;
   vs_out.uv_base = aUV;
   vs_out.uv_overlay = aUVOoverlay;
   vs_out.tint = aTint;
}