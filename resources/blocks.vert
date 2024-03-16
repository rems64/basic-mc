#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
uniform mat4 view_projection;
out vec3 Normal;
out vec2 UV;
void main()
{
   gl_Position = view_projection * vec4(aPos, 1.0);
   Normal = aNormal;
   UV = aUV;
}