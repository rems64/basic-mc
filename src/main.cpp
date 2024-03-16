#include <iostream>
#include <vector>
#include <unordered_map>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "std_image.h"

#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

using namespace std;

typedef struct Transform
{
    glm::vec3 position = {0, 0, 0};
    glm::vec3 scale = {1, 1, 1};
    glm::vec3 rotation = {0, 0, 0};

    glm::mat4 transform;
    bool dirty;
} Transform_t;

typedef struct Camera
{
    float fov;
    float near;
    float far;

    float freeflight_speed = 0.05f;

    glm::vec3 position;
    glm::vec3 direction;
    float yaw;
    float pitch;
    glm::vec3 up = {0, 0, 1};
} Camera_t;

typedef struct mInput
{
    double last_x = -1;
    double last_y = -1;
    bool mouse_capture = false;
} mInput_t;

typedef struct mContext
{
    int fps = 0;
    int dt = 0;
    uint32_t dc = 0;
    mInput_t input;
    Camera_t *main_camera = nullptr;
} mContext_t;

typedef uint32_t BlockId_t;
#define BLOCKID_AIR \
    BlockId_t { 0 }

typedef struct Block
{
    BlockId_t block_id;
} Block_t;

// Slice: 16x16x16
// Chunk: 16x16x384, 24 slices high

typedef struct Slice
{
    uint8_t z;
    uint8_t index;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    unsigned int vbo;
    unsigned int ebo;
    std::vector<Block_t> table;
    uint16_t blocks[4096];
} Slice_t;

typedef struct Chunk
{
    int64_t x;
    int64_t y;
    uint32_t section_x;
    uint32_t section_y;
    Slice_t slices[24];
} Chunk_t;

typedef struct Section
{
    int64_t x;
    int64_t y;
    Chunk_t *chunks[16 * 16];
} Section_t;

typedef struct World
{
    Section_t section;
} World_t;

// clang-format off
std::vector<float> cube_vertices = {
    -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 
     1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, 
     1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f, 
    -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, 
    -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, 
     1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, 
     1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, 
    -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f
};

std::vector<unsigned int> cube_indices = {
    0, 1, 3,
    1, 2, 3,
    1, 2, 5,
    2, 5, 6,
    0, 1, 4,
    4, 1, 5,
    3, 0, 7,
    7, 0, 4,
    2, 3, 7,
    6, 2, 7
};
// clang-format on

std::unordered_map<BlockId_t, std::pair<int, int>> blocks_uvs{
    {1, std::make_pair(6, 26)},
    {2, std::make_pair(10, 14)}};

const int TEXTURE_BLOCKS_WIDTH = 1024;
const int TEXTURE_BLOCKS_HEIGHT = 512;
const float TEXTURE_TILE_WIDTH = 16.f / TEXTURE_BLOCKS_WIDTH;
const float TEXTURE_TILE_HEIGHT = 16.f / TEXTURE_BLOCKS_HEIGHT;

void init_slice(Slice_t *slice)
{
    slice->table = {};
}

void init_chunk(Chunk_t *chunk, Section_t *section, int64_t x, int64_t y)
{
    chunk->section_x = x;
    chunk->section_y = y;
    chunk->x = section->x * 256 + 16 * x;
    chunk->y = section->y * 256 + 16 * y;
    for (uint8_t slice = 0; slice < 24; slice++)
    {
        init_slice(&chunk->slices[slice]);
    }
}

constexpr size_t block_index(uint8_t x, uint8_t y, uint8_t z)
{
    return x + 16 * y + 256 * z;
}

size_t chunk_id(uint32_t x, uint32_t y)
{
    return x + 16 * y;
}

inline int positive_mod(int i, int n)
{
    return (i % n + n) % n;
}

BlockId_t get_block_id(Section_t *section, Chunk_t *chunk, Slice_t *slice, int32_t x, int32_t y, int32_t z)
{
    int32_t slice_x = positive_mod(x, 16);
    int32_t slice_y = positive_mod(y, 16);
    int32_t slice_z = positive_mod(z, 16);
    Slice_t *concerned_slice = slice;
    if (x < 0)
    {
        if (chunk->section_x <= 0)
            return BLOCKID_AIR;
        Chunk_t *concerned_chunk = section->chunks[chunk_id(chunk->section_x - 1, chunk->section_y)];
        if (concerned_chunk == NULL)
            return BLOCKID_AIR;
        concerned_slice = &concerned_chunk->slices[slice->index];
        if (concerned_slice == NULL)
            return BLOCKID_AIR;
    }
    if (x > 15)
    {
        if (chunk->section_x >= 15)
            return BLOCKID_AIR;
        Chunk_t *concerned_chunk = section->chunks[chunk_id(chunk->section_x + 1, chunk->section_y)];
        if (concerned_chunk == NULL)
            return BLOCKID_AIR;
        concerned_slice = &concerned_chunk->slices[slice->index];
        if (concerned_slice == NULL)
            return BLOCKID_AIR;
    }
    if (y < 0)
    {
        if (chunk->section_y <= 0)
            return BLOCKID_AIR;
        Chunk_t *concerned_chunk = section->chunks[chunk_id(chunk->section_x, chunk->section_y - 1)];
        if (concerned_chunk == NULL)
            return BLOCKID_AIR;
        concerned_slice = &concerned_chunk->slices[slice->index];
        if (concerned_slice == NULL)
            return BLOCKID_AIR;
    }
    if (y > 15)
    {
        if (chunk->section_y >= 15)
            return BLOCKID_AIR;
        Chunk_t *concerned_chunk = section->chunks[chunk_id(chunk->section_x, chunk->section_y + 1)];
        if (concerned_chunk == NULL)
            return BLOCKID_AIR;
        concerned_slice = &concerned_chunk->slices[slice->index];
        if (concerned_slice == NULL)
            return BLOCKID_AIR;
    }
    if (z < 0)
    {
        if (slice->index <= 0)
            return BLOCKID_AIR;
        concerned_slice = &chunk->slices[slice->index - 1];
        if (concerned_slice == NULL)
            return BLOCKID_AIR;
    }
    if (z > 15)
    {
        if (slice->index >= 23)
            return BLOCKID_AIR;
        concerned_slice = &chunk->slices[slice->index + 1];
        if (concerned_slice == NULL)
            return BLOCKID_AIR;
    }
    const size_t blocks_count = concerned_slice->table.size();
    if (blocks_count == 0)
    {
        std::cout << "[ERROR] Trying to get the block id of a block within an empty chunk" << std::endl;
        return 0;
    }
    else if (blocks_count == 1)
    {
        return concerned_slice->table[0].block_id;
    }
    else
    {
        return concerned_slice->table[concerned_slice->blocks[block_index(slice_x, slice_y, slice_z)]].block_id;
    }
}

void generate_chunk(Chunk_t *chunk)
{
    for (size_t slice_index = 0; slice_index < 12; slice_index++)
    {
        Slice_t *slice = &chunk->slices[slice_index];
        slice->table.push_back(Block_t{0}); // AIR
        slice->table.push_back(Block_t{1}); // STONE
        slice->table.push_back(Block_t{2}); // COAL
        slice->index = slice_index;
        slice->z = 16 * slice_index;
        for (size_t x = 0; x < 16; x++)
        {
            for (size_t y = 0; y < 16; y++)
            {
                float scale = 0.1f;
                uint8_t height = 32.f +
                                 stb_perlin_noise3(scale * (x + chunk->x), scale * (y + chunk->y), 0.f, 0, 0, 0) * 5.f +
                                 stb_perlin_noise3(0.2f * scale * (x + chunk->x), 0.2f * scale * (y + chunk->y), 0.f, 0, 0, 0) * 10.f;
                for (size_t z = 0; z < 16; z++)
                {
                    float scale = 0.05f;
                    float cave = stb_perlin_noise3(scale * (x+chunk->x), scale*(y+chunk->y), scale*(z+slice->z), 0, 0, 0);
                    bool air = (slice->z + z) >= height || cave>=0.4f;
                    if (air)
                    {
                        slice->blocks[block_index(x, y, z)] = 0;
                    }
                    else
                    {
                        const float r = (float)rand() / (float)RAND_MAX;
                        const bool coal = r > 0.8f;
                        slice->blocks[block_index(x, y, z)] = coal ? 2 : 1;
                    }
                }
            }
        }
    }
}

void push_indices(std::vector<unsigned int> *indices, size_t offset, float normal_direction)
{
    if (normal_direction > 0)
    {
        indices->push_back(offset + 2);
        indices->push_back(offset + 1);
        indices->push_back(offset + 0);
        indices->push_back(offset + 3);
        indices->push_back(offset + 2);
        indices->push_back(offset + 0);
    }
    else
    {
        indices->push_back(offset + 0);
        indices->push_back(offset + 1);
        indices->push_back(offset + 2);
        indices->push_back(offset + 0);
        indices->push_back(offset + 2);
        indices->push_back(offset + 3);
    }
}

void add_face_x(std::vector<float> *vertices, std::vector<unsigned int> *indices, int64_t x, int64_t y, int64_t z, float slice_height, float normal_direction, float uv_x, float uv_y)
{
    size_t offset = vertices->size() / 8;
    // clang-format off
    const std::vector<float> new_vertices = {
        (float)x, (float)y, (float)z, (float)normal_direction, 0.f, 0.f, 0.f+uv_x, TEXTURE_TILE_HEIGHT+uv_y,
        (float)x, (float)y+1.f, (float)z, (float)normal_direction, 0.f, 0.f, TEXTURE_TILE_WIDTH+uv_x, TEXTURE_TILE_HEIGHT+uv_y,
        (float)x, (float)y+1.f, (float)z+1.f, (float)normal_direction, 0.f, 0.f, TEXTURE_TILE_WIDTH+uv_x, 0.f+uv_y,
        (float)x, (float)y, (float)z+1.f, (float)normal_direction, 0.f, 0.f, 0.f+uv_x, 0.f+uv_y
    };
    // clang-format on

    vertices->reserve(vertices->size() + distance(new_vertices.begin(), new_vertices.end()));
    vertices->insert(vertices->end(), new_vertices.begin(), new_vertices.end());

    push_indices(indices, offset, -normal_direction);
}

void add_face_y(std::vector<float> *vertices, std::vector<unsigned int> *indices, int64_t x, int64_t y, int64_t z, float slice_height, float normal_direction, float uv_x, float uv_y)
{
    size_t offset = vertices->size() / 8;
    // clang-format off
    const std::vector<float> new_vertices = {
        (float)x, (float)y, (float)z, 0.f, (float)normal_direction, 0.f, 0.f+uv_x, TEXTURE_TILE_HEIGHT+uv_y,
        (float)x+1.f, (float)y, (float)z, 0.f, (float)normal_direction, 0.f, TEXTURE_TILE_WIDTH+uv_x, TEXTURE_TILE_HEIGHT+uv_y,
        (float)x+1.f, (float)y, (float)z+1.f, 0.f, (float)normal_direction, 0.f, TEXTURE_TILE_WIDTH+uv_x, 0.f+uv_y,
        (float)x, (float)y, (float)z+1.f, 0.f, (float)normal_direction, 0.f, 0.f+uv_x, 0.f+uv_y
    };
    // clang-format on

    vertices->reserve(vertices->size() + std::distance(new_vertices.begin(), new_vertices.end()));
    vertices->insert(vertices->end(), new_vertices.begin(), new_vertices.end());

    push_indices(indices, offset, normal_direction);
}

void add_face_z(std::vector<float> *vertices, std::vector<unsigned int> *indices, int64_t x, int64_t y, int64_t z, float slice_height, float normal_direction, float uv_x, float uv_y)
{
    size_t offset = vertices->size() / 8;
    // clang-format off
    const std::vector<float> new_vertices = {
        (float)x, (float)y, (float)z, 0.f, 0.f, (float)normal_direction, 0.f+uv_x, 0.f+uv_y,
        (float)x+1.f, (float)y, (float)z, 0.f, 0.f, (float)normal_direction, TEXTURE_TILE_WIDTH+uv_x, 0.f+uv_y,
        (float)x+1.f, (float)y+1.f, (float)z, 0.f, 0.f, (float)normal_direction, TEXTURE_TILE_WIDTH+uv_x, TEXTURE_TILE_HEIGHT+uv_y,
        (float)x, (float)y+1.f, (float)z, 0.f, 0.f, (float)normal_direction, 0.f+uv_x, TEXTURE_TILE_HEIGHT+uv_y
    };
    // clang-format on

    vertices->reserve(vertices->size() + distance(new_vertices.begin(), new_vertices.end()));
    vertices->insert(vertices->end(), new_vertices.begin(), new_vertices.end());

    push_indices(indices, offset, -normal_direction);
}

void generate_slice_mesh(World_t *world, std::vector<float> *vertices, std::vector<unsigned int> *indices, Chunk_t *chunk, uint8_t slice_index)
{
    Section_t *section = &world->section;
    Slice *slice = &chunk->slices[slice_index];
    float slice_x = chunk->x;
    float slice_y = chunk->y;
    float slice_z = slice->z;
    for (size_t x = 0; x < 16; x++)
    {
        for (size_t y = 0; y < 16; y++)
        {
            for (size_t z = 0; z < 16; z++)
            {
                BlockId_t current_block_id = get_block_id(section, chunk, slice, x, y, z);
                // If air
                if (current_block_id == 0)
                {
                    continue;
                }
                bool g_top = (get_block_id(section, chunk, slice, x, y, z + 1) == 0);
                bool g_bottom = (get_block_id(section, chunk, slice, x, y, z - 1) == 0);
                bool g_left = (get_block_id(section, chunk, slice, x - 1, y, z) == 0);
                bool g_right = (get_block_id(section, chunk, slice, x + 1, y, z) == 0);
                bool g_front = (get_block_id(section, chunk, slice, x, y - 1, z) == 0);
                bool g_back = (get_block_id(section, chunk, slice, x, y + 1, z) == 0);

                const std::pair<int, int> uv_offset = blocks_uvs[current_block_id];
                float uv_offset_x = 16.f * uv_offset.first / TEXTURE_BLOCKS_WIDTH;
                float uv_offset_y = 16.f * uv_offset.second / TEXTURE_BLOCKS_HEIGHT;
                if (g_left)
                {
                    add_face_x(vertices, indices, x + slice_x, y + slice_y, z + slice_z, slice_z, -1, uv_offset_x, uv_offset_y);
                }
                if (g_right)
                {
                    add_face_x(vertices, indices, x + slice_x + 1, y + slice_y, z + slice_z, slice_z, 1, uv_offset_x, uv_offset_y);
                }
                if (g_front)
                {
                    add_face_y(vertices, indices, x + slice_x, y + slice_y, z + slice_z, slice_z, -1, uv_offset_x, uv_offset_y);
                }
                if (g_back)
                {
                    add_face_y(vertices, indices, x + slice_x, y + slice_y + 1, z + slice_z, slice_z, 1, uv_offset_x, uv_offset_y);
                }
                if (g_bottom)
                {
                    add_face_z(vertices, indices, x + slice_x, y + slice_y, z + slice_z, slice_z, -1, uv_offset_x, uv_offset_y);
                }
                if (g_top)
                {
                    add_face_z(vertices, indices, x + slice_x, y + slice_y, z + slice_z + 1, slice_z, 1, uv_offset_x, uv_offset_y);
                }
            }
        }
    }
}

static mContext_t C;

void buildUi()
{
    ImGui::Begin("stats");
    ImGui::Text("FPS %i", C.fps);
    ImGui::Text("dt %ims", C.dt);
    ImGui::Text("draw count %i", C.dc);
    ImGui::End();
}

char *readfile(const char *filepath)
{
    FILE *fp;
    fopen_s(&fp, filepath, "r");
    if (!fp)
    {
        printf("[ERROR] Failed to open %s", filepath);
        return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    long lSize = ftell(fp);
    rewind(fp);
    char *buffer = (char *)calloc(1, lSize + 1);
    if (!buffer)
    {
        printf("[ERROR] Failed to allocate memory for file %s", filepath);
        return NULL;
    }
    fread(buffer, lSize, 1, fp);
    return buffer;
}

void create_shader(const char *vertex_path, const char *fragment_path, unsigned int *program)
{
    *program = glCreateProgram();
    char *vertex_shader_source = readfile(vertex_path);
    unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);
    free(vertex_shader_source);

    char *fragment_shader_source = readfile(fragment_path);
    unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);
    free(fragment_shader_source);

    glAttachShader(*program, vertex_shader);
    glAttachShader(*program, fragment_shader);
    glLinkProgram(*program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
}

void update_transform(Transform_t *transform)
{
    if (!transform->dirty)
        return;
    transform->transform = glm::translate(glm::mat4(1.f), transform->position) *
                           glm::rotate(glm::mat4(1.f), transform->rotation.b, glm::vec3(0, 0, 1)) *
                           glm::rotate(glm::mat4(1.f), transform->rotation.r, glm::vec3(1, 0, 0)) *
                           glm::rotate(glm::mat4(1.f), transform->rotation.g, glm::vec3(0, 0, 1)) *
                           glm::scale(glm::mat4(1.f), transform->scale);
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    ImGuiIO &io = ImGui::GetIO();
    io.AddMousePosEvent(xpos, ypos);
    if (C.input.mouse_capture)
    {
        float xoffset = xpos - C.input.last_x;
        float yoffset = C.input.last_y - ypos;
        C.input.last_x = xpos;
        C.input.last_y = ypos;

        float sensitivity = 0.1f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        C.main_camera->yaw += xoffset;
        C.main_camera->pitch += yoffset;

        if (C.main_camera->pitch > 89.0f)
            C.main_camera->pitch = 89.0f;
        if (C.main_camera->pitch < -89.0f)
            C.main_camera->pitch = -89.0f;
    }
}

void set_capture_cursor(GLFWwindow *window, bool capture)
{
    if (capture)
    {
        C.input.mouse_capture = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetCursorPos(window, C.input.last_x, C.input.last_y);
    }
    else
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        C.input.mouse_capture = false;
    }
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    ImGuiIO &io = ImGui::GetIO();
    io.AddMouseButtonEvent(button, action == GLFW_PRESS);
    if (!C.input.mouse_capture && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && !io.WantCaptureMouse)
    {
        set_capture_cursor(window, true);
    }
}

void mousewheel_callback(GLFWwindow *window, double wheel_x, double wheel_y)
{
    ImGuiIO &io = ImGui::GetIO();
    io.AddMouseWheelEvent(wheel_x, wheel_y);
    if (!io.WantCaptureMouse)
    {
        if (wheel_y < 0)
        {
            C.main_camera->freeflight_speed /= 2;
        }
        else
        {
            C.main_camera->freeflight_speed *= 2;
        }
    }
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (C.input.mouse_capture)
    {
        Camera_t *camera = C.main_camera;
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            set_capture_cursor(window, false);
        }
    }
}

void update_player(GLFWwindow *window)
{
    Camera_t *camera = C.main_camera;
    double dt = C.dt;
    glm::vec3 offset = glm::vec3(0.f);
    glm::vec3 forward = camera->direction;
    glm::vec3 right = glm::normalize(glm::cross(camera->direction, camera->up));
    float speed = dt * camera->freeflight_speed;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        offset += camera->direction;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        offset += -camera->direction;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        offset += -right;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        offset += right;
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    {
        offset += glm::vec3(0, 0, 1);
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    {
        offset += -glm::vec3(0, 0, 1);
    }
    if (glm::length(offset) > 0.001f)
    {
        camera->position += speed * glm::normalize(offset);
    }
}

void init_world(World_t *world)
{
    world->section.x = 0;
    world->section.y = 0;
    for (size_t chunk_id = 0; chunk_id < sizeof(world->section.chunks) / sizeof(Chunk_t *); chunk_id++)
    {
        world->section.chunks[chunk_id] = nullptr;
    }
}

int main(int argc, char **argv)
{
    if (!glfwInit())
    {
        std::cout << "Failed to initialize GLFW" << std::endl;
    }

    static int screen_width = 1280;
    static int screen_height = 720;

    GLFWwindow *window = glfwCreateWindow(screen_width, screen_height, "Minecraft mais beaucoup moins bien et avec plein de bugs", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    gladLoadGL();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    ImGui_ImplGlfw_InitForOpenGL(window, true); // Second param install_callback=true will install GLFW callbacks and chain to existing ones.
    ImGui_ImplOpenGL3_Init();

    double last_time = glfwGetTime();

    // Shader creation
    unsigned int cube_shader_program;
    create_shader("resources/blocks.vert", "resources/blocks.frag", &cube_shader_program);

    // Texture
    int texture_width, texture_height, texture_depth;
    unsigned char *texture_data = stbi_load("resources/blocks.png", &texture_width, &texture_height, &texture_depth, 0);
    if (!texture_data)
    {
        printf("Failed to load blocks.png\n");
        exit(EXIT_FAILURE);
    }
    printf("Loaded blocks.png (%i, %i, %i)\n", texture_width, texture_height, texture_depth);

    unsigned int blocks_texture;
    glGenTextures(1, &blocks_texture);
    glBindTexture(GL_TEXTURE_2D, blocks_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_width, texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(texture_data);

    // Shader select
    glUseProgram(cube_shader_program);

    Camera_t camera = {};
    camera.fov = 60.f / 180.f * glm::pi<float>();
    camera.near = 0.1;
    camera.far = 1000.;
    camera.position = glm::vec3(-10.f, -10.f, 10.f);
    C.main_camera = &camera;

    // Uniforms
    int modelLoc = glGetUniformLocation(cube_shader_program, "view_projection");

    World_t world;
    init_world(&world);

    for (int i = 0; i < 10; i++)
    {
        for (int j = 0; j < 10; j++)
        {
            // Test chunk removal
            // if (i==4 && j==3) continue;
            Chunk_t *chunk = new Chunk_t;
            init_chunk(chunk, &world.section, j, i);
            generate_chunk(chunk);
            world.section.chunks[chunk_id(j, i)] = chunk;
        }
    }
    for (int i = 0; i < 16; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            Chunk_t *chunk = world.section.chunks[chunk_id(j, i)];
            if (chunk == NULL)
                continue;
            for (size_t slice_index = 0; slice_index < 4; slice_index++)
            {
                Slice_t *slice = &chunk->slices[slice_index];
                generate_slice_mesh(&world, &slice->vertices, &slice->indices, chunk, slice_index);
                // VBO
                glGenBuffers(1, &slice->vbo);
                glBindBuffer(GL_ARRAY_BUFFER, slice->vbo);
                glBufferData(GL_ARRAY_BUFFER, slice->vertices.size() * sizeof(float), slice->vertices.data(), GL_STATIC_DRAW);

                // EBO
                glGenBuffers(1, &slice->ebo);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, slice->ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, slice->indices.size() * sizeof(unsigned int), slice->indices.data(), GL_STATIC_DRAW);
            }
        }
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    float t = 0;

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, mousewheel_callback);
    glfwSetKeyCallback(window, key_callback);

    double target_fps = 60.f;
    double last_frame_time = 0.f;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        C.dc = 0;
        double current_time = glfwGetTime();
        double elapsed_time = current_time - last_time;
        if (elapsed_time < 1 / target_fps)
        {
            continue;
        }
        C.fps = int(1. / elapsed_time);
        C.dt = int(1000. * elapsed_time);
        t += elapsed_time;

        update_player(window);
        Camera_t *camera = C.main_camera;
        camera->direction = {cos(glm::radians(camera->yaw)) * cos(glm::radians(camera->pitch)), -sin(glm::radians(camera->yaw)) * cos(glm::radians(camera->pitch)), sin(glm::radians(camera->pitch))};
        glm::mat4 view = glm::lookAt(camera->position, camera->position + camera->direction, camera->up);
        glm::mat4 projection = glm::perspective(camera->fov, (float)screen_width / (float)screen_height, camera->near, camera->far);
        glm::mat4 VP = projection * view;
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(VP));

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        glfwGetFramebufferSize(window, &screen_width, &screen_height);
        glViewport(0, 0, screen_width, screen_height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Rendering goes here
        for (size_t i = 0; i < sizeof(world.section.chunks) / sizeof(Chunk_t *); i++)
        {
            if (world.section.chunks[i] == nullptr)
                continue;
            Chunk_t *chunk = world.section.chunks[i];
            for (size_t slice_index = 0; slice_index < 16; slice_index++)
            {
                Slice_t *slice = &chunk->slices[slice_index];
                if (slice == nullptr)
                    continue;
                const size_t count = slice->indices.size();
                glBindTexture(GL_TEXTURE_2D, blocks_texture);
                glBindBuffer(GL_ARRAY_BUFFER, slice->vbo);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, slice->ebo);

                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
                glEnableVertexAttribArray(0);
                glEnableVertexAttribArray(1);
                glEnableVertexAttribArray(2);
                glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0);
                C.dc++;
            }
        }

        buildUi();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        last_time = current_time;
    }
    for (size_t i = 0; i < sizeof(world.section.chunks) / sizeof(Chunk_t *); i++)
    {
        if (world.section.chunks[i] != nullptr)
        {
            free(world.section.chunks[i]);
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
