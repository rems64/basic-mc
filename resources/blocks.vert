#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec2 aUVOoverlay;
layout (location = 4) in vec3 aTint;
uniform mat4 view_projection;
out vec3 Normal;
out vec2 UV;
out vec2 UVOverlay;
out vec3 Tint;
void main()
{
   gl_Position = view_projection * vec4(aPos, 1.0);
   Normal = aNormal;
   UV = aUV;
   UVOverlay = aUVOoverlay;
   Tint = aTint;
}