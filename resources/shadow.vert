#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec2 aUVOoverlay;
layout (location = 4) in vec3 aTint;

uniform mat4 light_space_matrix;

void main()
{
    gl_Position = light_space_matrix * vec4(aPos, 1.0);
}