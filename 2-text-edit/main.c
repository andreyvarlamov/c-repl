#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "cglm/cglm.h"
#include "glad/glad.h"
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

enum { SCREEN_WIDTH = 800, SCREEN_HEIGHT = 600 };
enum { MAX_VERT = 1024, MAX_IDX = 4096 };
enum { ONE_MB = 1024 * 1024 };
enum { BAKED_BASE_ASCII = 32, BAKED_GLYPH_COUNT = 95 }; // ASCII Range: [32, 126]

typedef struct Texture {
    uint32_t id;
    float w, h;
} Texture;

typedef struct Rect {
    float x, y;
    float w, h;
} Rect;

typedef struct Window_State {
    int w, h;
    GLFWwindow *glfw_window;
} Window_State;

typedef struct Gl_State {
    uint32_t vbo;
    uint32_t ebo;
    uint32_t vao;
    uint32_t shader;
    Texture empty_texture;
} Gl_State;

typedef struct Baked_Font {
    uint8_t *rgba_bytes;
    Texture tex;
    stbtt_bakedchar glyph_metrics[BAKED_GLYPH_COUNT];
    size_t base_ascii;
    size_t glyph_count;
    float points_height;
} Baked_Font;

typedef struct Text_Edit_State {
    char text_buffer[ONE_MB];
    size_t text_buffer_cursor;
    size_t used_size;
    const char *file_name;
    int notify_frames;
} Text_Edit_State;

static Gl_State g_gl_state;
static Window_State g_window_state;
static char gl_error_buffer[ONE_MB];
static Text_Edit_State  g_text_edit_state = {0};

void exit_with_error(const char *msg, ...);
void trace_log(const char *msg, ...);
void *xmalloc(size_t bytes);
void *xcalloc(size_t bytes);

void keyboard_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
void char_callback(GLFWwindow* window, uint32_t codepoint);
void window_size_callback(GLFWwindow *window, int width, int height);

uint32_t build_shader_from_src(const char *src, GLenum shader_type);
uint32_t link_vert_frag_shaders(uint32_t vert, uint32_t frag);
uint32_t build_default_shaders();

Gl_State initialize_gl_state();
void set_ortho_projection(int width, int height);

Texture load_texture(const char *file);
Texture load_empty_texture();

void draw_texture(Rect dest, Texture texture, Rect src, vec4 color);
void draw_texture_scaled(vec2 pos, Texture texture, float scale);
void draw_texture_scaled_tinted(vec2 pos, Texture texture, float scale, vec4 color);
void draw_quad(Rect quad, vec4 color);

Baked_Font bake_font(const char *file_name, float points_height, int atlas_dim);
void test_bake_font_to_png(const char *font_file_name, const char *out_png_file_name, float points_height, int atlas_dim);
Baked_Font bake_font_to_texture(const char *file, float points_height, int atlas_dim);
void draw_string(const char *str, vec2 pos, vec4 color, Baked_Font font, float line_height);
void draw_string_with_cursor(const char *str, size_t cursor, vec2 pos, vec4 color, Baked_Font font, float line_height);

void handle_input_char(uint32_t c);
void handle_backspace_char();
void advance_cursor(bool forward);
void save_file();
void try_load_file();

int main(int argc, char **argv) {
    if (!glfwInit()) {
        exit_with_error("Failed to initialize GLFW");
    }

    trace_log("GLFW initialized");

    // Set GLFW OpenGL version -- 4.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    g_window_state.w = SCREEN_WIDTH;
    g_window_state.h = SCREEN_HEIGHT;
    g_window_state.glfw_window = glfwCreateWindow(g_window_state.w, g_window_state.h, "Text Edit Proto", NULL, NULL);
    if (g_window_state.glfw_window == NULL) {
        exit_with_error("Failed to create GLFW window");
    }

    trace_log("GLFW window created");

    glfwMakeContextCurrent(g_window_state.glfw_window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        exit_with_error("Failed to load GL function pointers");
    }

    trace_log("Loaded OpenGL function pointers. Debug info:");
    trace_log("  Version:  %s", glGetString(GL_VERSION));
    trace_log("  GLSL:     %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
    trace_log("  Vendor:   %s", glGetString(GL_VENDOR));
    trace_log("  Renderer: %s", glGetString(GL_RENDERER));

    glfwSetKeyCallback(g_window_state.glfw_window, keyboard_callback);
    glfwSetWindowSizeCallback(g_window_state.glfw_window, window_size_callback);
    glfwSetCharCallback(g_window_state.glfw_window, char_callback);
    g_gl_state = initialize_gl_state();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glClearColor(0.09f, 0.07f, 0.07f, 1.0f);

    set_ortho_projection(SCREEN_WIDTH, SCREEN_HEIGHT);

    test_bake_font_to_png("res/ubuntu_mono.ttf", "temp/baked_font.png", 32.0f, 512);

    Texture claesz = load_texture("res/claesz.png");
    Baked_Font baked_font = bake_font_to_texture("res/ubuntu_mono.ttf", 32.0f, 512);

    if (argc < 2) {
        g_text_edit_state.file_name = "temp/from_editor.c";
    } else {
        g_text_edit_state.file_name = argv[1];
    }

    try_load_file();

    trace_log("Editing file: %s", g_text_edit_state.file_name);

    trace_log("Entering main loop");
    while (!glfwWindowShouldClose(g_window_state.glfw_window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        float bg_scale = 0.7f;
        vec2 bg_pos = {
            g_window_state.w * 0.5f - claesz.w * bg_scale * 0.5f,
            g_window_state.h * 0.5f - claesz.h * bg_scale * 0.5f
        };
        draw_texture_scaled_tinted(bg_pos, claesz, bg_scale, (vec4){0.22f, 0.2f, 0.2f, 0.5f});

        draw_string_with_cursor(g_text_edit_state.text_buffer,
                                g_text_edit_state.text_buffer_cursor,
                                (vec2){20.0f, 50.0f},
                                (vec4){0.76f, 0.8f, 0.8f, 0.8f},
                                baked_font,
                                baked_font.points_height);

        if (g_text_edit_state.notify_frames > 0) {
            g_text_edit_state.notify_frames--;
            float dim = 50.0f;
            draw_quad((Rect){g_window_state.w - dim, g_window_state.h - dim, dim, dim}, (vec4){0.6f, 0.55f, 0.55f, 0.6f});
        }

        glfwSwapBuffers(g_window_state.glfw_window);
        glfwPollEvents();
    }

    trace_log("GLFW terminating gracefully");

    glfwTerminate();
    return 0;
}

void exit_with_error(const char *msg, ...) {
    fprintf(stderr, "FATAL: ");
    va_list ap;
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fprintf(stderr, "\n");

    glfwTerminate();
    exit(1);
}

void trace_log(const char *msg, ...) {
    printf("INFO: ");
    va_list ap;
    va_start(ap, msg);
    vprintf(msg, ap);
    va_end(ap);
    printf("\n");
}

void *xmalloc(size_t bytes) {
    void *d = malloc(bytes);
    if (d == NULL) exit_with_error("Failed to malloc");
    return d;
}
void *xcalloc(size_t bytes) {
    void *d = calloc(1, bytes);
    if (d == NULL) exit_with_error("Failed to calloc");
    return d;
}

void keyboard_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    (void)window; (void)key; (void)scancode; (void)action; (void)mods;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        trace_log("Received ESC. Terminating...");
        glfwSetWindowShouldClose(window, true);
    } else if (key == GLFW_KEY_ENTER && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        handle_input_char('\n');
    } else if (key == GLFW_KEY_BACKSPACE && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        handle_backspace_char();
    } else if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        advance_cursor(false);
    } else if (key == GLFW_KEY_RIGHT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        advance_cursor(true);
    } else if (key == GLFW_KEY_S && (action == GLFW_PRESS) && (mods & GLFW_MOD_CONTROL)) {
        save_file();
    }
}

void char_callback(GLFWwindow* window, uint32_t codepoint) {
    (void)window;
    if (codepoint < 0x100) {
        handle_input_char(codepoint);
    } else {
        trace_log("Unhandled codepoint: 0x%08X", codepoint);
    }
}

void window_size_callback(GLFWwindow *window, int width, int height) {
    (void)window;

    g_window_state.w = width;
    g_window_state.h = height;
    glViewport(0, 0, width, height);
    set_ortho_projection(width, height);
}


uint32_t build_shader_from_src(const char *src, GLenum shader_type) {
    uint32_t id = glCreateShader(shader_type);
    glShaderSource(id, 1, &src, NULL);
    glCompileShader(id);

    int success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);

    if (!success) {
        glGetShaderInfoLog(id, ONE_MB, NULL, gl_error_buffer);
        exit_with_error("Failed to compile shader (type 0x%04X). Error:\n  %s\nSource:\n%s\n", shader_type, gl_error_buffer, src);
    }

    return id;
}

uint32_t link_vert_frag_shaders(uint32_t vert, uint32_t frag) {
    uint32_t id = glCreateProgram();
    glAttachShader(id, vert);
    glAttachShader(id, frag);
    glLinkProgram(id);

    int success;
    glGetProgramiv(id, GL_LINK_STATUS, &success);

    if (!success) {
        glGetProgramInfoLog(id, ONE_MB, NULL, gl_error_buffer);
        exit_with_error("Failed to compile program. Error:\n  %s", gl_error_buffer);
    }

    return id;
}

uint32_t build_default_shaders() {
    static const char *vert_shader_source =
        "#version 430 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "layout (location = 2) in vec4 aColor;\n"
        "uniform mat4 projection;\n"
        "out vec2 TexCoord;\n"
        "out vec4 Color;\n"
        "void main() {\n"
        "    gl_Position = projection * vec4(aPos, 0.0, 1.0);\n"
        "    TexCoord = aTexCoord;\n"
        "    Color = aColor;\n"
        "}";
    uint32_t vert_shader = build_shader_from_src(vert_shader_source, GL_VERTEX_SHADER);

    static const char *frag_shader_source =
        "#version 430 core\n"
        "out vec4 FragColor;\n"
        "in vec2 TexCoord;\n"
        "in vec4 Color;\n"
        "uniform sampler2D texture1;\n"
        "void main() {\n"
        "    FragColor = Color * texture(texture1, TexCoord);\n"
        "}";
    uint32_t frag_shader = build_shader_from_src(frag_shader_source, GL_FRAGMENT_SHADER);

    uint32_t shader_program = link_vert_frag_shaders(vert_shader, frag_shader);

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    return shader_program;
}

Gl_State initialize_gl_state() {
    Gl_State gl_state = {0};

    glGenVertexArrays(1, &gl_state.vao);
    glGenBuffers(1, &gl_state.vbo);
    glGenBuffers(1, &gl_state.ebo);

    glBindVertexArray(gl_state.vao);

    glBindBuffer(GL_ARRAY_BUFFER, gl_state.vbo);

    size_t total_size = MAX_VERT * (2 + 2 + 4) * sizeof(float);
    glBufferData(GL_ARRAY_BUFFER, total_size, NULL, GL_STREAM_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_state.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_IDX * sizeof(uint32_t), NULL, GL_STREAM_DRAW);

    // Positions -- vec2
    size_t stride = 2 * sizeof(float);
    size_t offset = 0;
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void *)offset);
    glEnableVertexAttribArray(0);

    // TexCoords -- vec2
    offset += stride * MAX_VERT;
    stride = 2 * sizeof(float);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)offset);
    glEnableVertexAttribArray(1);

    // Color -- vec4
    offset += stride * MAX_VERT;
    stride = 4 * sizeof(float);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void *)offset);
    glEnableVertexAttribArray(2);

    offset += stride * MAX_VERT;
    assert(offset == total_size);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    gl_state.shader = build_default_shaders();


    gl_state.empty_texture = load_empty_texture();

    return gl_state;
}

void set_ortho_projection(int width, int height) {
    mat4 projection;
    glm_ortho(0.0f, width, height, 0.0f, -1.0f, 1.0f, projection);

    glUseProgram(g_gl_state.shader);
    glUniformMatrix4fv(glGetUniformLocation(g_gl_state.shader, "projection"), 1, GL_FALSE, (float *)projection);
    glUseProgram(0);
}

Texture load_texture(const char *file) {
    Texture texture = {0};

    glGenTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D, texture.id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(false);
    int width, height, channel_count;
    uint8_t *image_data = stbi_load(file, &width, &height, &channel_count, STBI_rgb_alpha);
    if (image_data == NULL) {
        exit_with_error("Failed to load image at %s", file);
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(image_data);

    texture.w = (float)width;
    texture.h = (float)height;

    return texture;
}

Texture load_empty_texture() {
    Texture texture = {0};
    texture.w = texture.h = 1;

    glGenTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D, texture.id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    uint32_t white = -1;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);

    glBindTexture(GL_TEXTURE_2D, 0);

    return texture;
}

void draw_texture(Rect dest, Texture texture, Rect src, vec4 color) {
    glBindBuffer(GL_ARRAY_BUFFER, g_gl_state.vbo);

    size_t total_size = MAX_VERT * (2 + 2 + 4) * sizeof(float);
    size_t vert_to_sub_count = 4;
    size_t idx_to_sub_count = 6;
    assert(vert_to_sub_count < MAX_VERT);
    assert(idx_to_sub_count < MAX_IDX);
    size_t stride, offset;

    // Positions -- vec2
    offset = 0;
    stride = 2 * sizeof(float);
    float positions[] = {
        dest.x, dest.y,
        dest.x + dest.w, dest.y,
        dest.x, dest.y + dest.h,
        dest.x + dest.w, dest.y + dest.h
    };
    assert(sizeof(positions) == stride * vert_to_sub_count);
    glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(positions), positions);

    // TexCoords -- vec2
    offset += stride * MAX_VERT;
    stride = 2 * sizeof(float);
    Rect src_norm = (Rect){src.x / texture.w, src.y / texture.h, src.w / texture.w, src.h / texture.h};
    float tex_coords[] = {
        src_norm.x, src_norm.y,
        src_norm.x + src_norm.w, src_norm.y,
        src_norm.x, src_norm.y + src_norm.h,
        src_norm.x + src_norm.w, src_norm.y + src_norm.h
    };
    assert(sizeof(tex_coords) == stride * vert_to_sub_count);
    glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(tex_coords), tex_coords);

    // Color -- vec4
    offset += stride * MAX_VERT;
    stride = 4 * sizeof(float);
    float colors[16];
    for (int i = 0; i < 4; i++) memcpy(colors + i * 4, color, 4 * sizeof(float));
    assert(sizeof(colors) == stride * vert_to_sub_count);
    glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(colors), colors);

    offset += stride * MAX_VERT;
    assert(offset == total_size);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_gl_state.ebo);

    uint32_t indices[] = {
        2, 1, 0,
        2, 3, 1
    };
    assert(sizeof(indices) / sizeof(indices[0]) == idx_to_sub_count);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(indices), indices);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glUseProgram(g_gl_state.shader);
    glBindVertexArray(g_gl_state.vao);
    glBindTexture(GL_TEXTURE_2D, texture.id);

    glDrawElements(GL_TRIANGLES, idx_to_sub_count, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void draw_texture_scaled(vec2 pos, Texture texture, float scale) {
    vec2 size = {texture.w, texture.h};
    glm_vec2_scale(size, scale, size);
    draw_texture((Rect){pos[0], pos[1], size[0], size[1]},
                 texture,
                 (Rect){0.0f, 0.0f, texture.w, texture.h},
                 (vec4){1.0f, 1.0f, 1.0f, 1.0f});
}

void draw_texture_scaled_tinted(vec2 pos, Texture texture, float scale, vec4 color) {
    vec2 size = {texture.w, texture.h};
    glm_vec2_scale(size, scale, size);
    draw_texture((Rect){pos[0], pos[1], size[0], size[1]},
                 texture,
                 (Rect){0.0f, 0.0f, texture.w, texture.h},
                 color);
}

void draw_quad(Rect quad, vec4 color) {
    draw_texture(quad, g_gl_state.empty_texture, (Rect){0}, color);
}

Baked_Font bake_font(const char *file_name, float points_height, int atlas_dim) {
    Baked_Font baked_font = {0};
    baked_font.base_ascii = BAKED_BASE_ASCII;
    baked_font.glyph_count = BAKED_GLYPH_COUNT;

    FILE *font_file = fopen(file_name, "rb");
    if (!font_file) exit_with_error("Failed to load font at %s", file_name);

    fseek(font_file, 0, SEEK_END);
    size_t font_size = ftell(font_file);
    rewind(font_file);

    uint8_t *font_bytes = xmalloc(font_size);
    fread(font_bytes, 1, font_size, font_file);
    fclose(font_file);

    size_t pixel_count = atlas_dim * atlas_dim;
    uint8_t *atlas_gray_bytes = xcalloc(pixel_count);

    int result = stbtt_BakeFontBitmap(font_bytes, 0, points_height,
                                      atlas_gray_bytes, atlas_dim, atlas_dim,
                                      baked_font.base_ascii, baked_font.glyph_count, baked_font.glyph_metrics);
    if (result <= 0) exit_with_error("Failed to bake font bitmap. stbtt_BakeFontBitmap(...) = %d", result);

    free(font_bytes);

    uint8_t *atlas_rgba_bytes = xmalloc(pixel_count * 4);

    for (size_t i = 0; i < pixel_count; i++) {
        atlas_rgba_bytes[i * 4 + 0] = atlas_gray_bytes[i];
        atlas_rgba_bytes[i * 4 + 1] = atlas_gray_bytes[i];
        atlas_rgba_bytes[i * 4 + 2] = atlas_gray_bytes[i];
        atlas_rgba_bytes[i * 4 + 3] = atlas_gray_bytes[i];
    }

    free(atlas_gray_bytes);

    baked_font.rgba_bytes = atlas_rgba_bytes;
    baked_font.tex.w = atlas_dim;
    baked_font.tex.h = atlas_dim;
    baked_font.points_height = points_height;

    return baked_font;
}

void test_bake_font_to_png(const char *font_file_name, const char *out_png_file_name, float points_height, int atlas_dim) {
    Baked_Font baked_font = bake_font(font_file_name, points_height, atlas_dim);
    int write_png_result = stbi_write_png(out_png_file_name,
                                          atlas_dim,
                                          atlas_dim,
                                          1,
                                          baked_font.rgba_bytes,
                                          atlas_dim);
    if (!write_png_result) {
        exit_with_error("Failed to write baked atlas to %s", out_png_file_name);
    }
    trace_log("test_bake_font_to_png: wrote baked atlas png to %s", out_png_file_name);
    free(baked_font.rgba_bytes);
}

Baked_Font bake_font_to_texture(const char *file, float points_height, int atlas_dim) {
    Baked_Font baked_font = bake_font(file, points_height, atlas_dim);

    glGenTextures(1, &baked_font.tex.id);
    glBindTexture(GL_TEXTURE_2D, baked_font.tex.id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, baked_font.tex.w, baked_font.tex.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, baked_font.rgba_bytes);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);

    return baked_font;
}

void draw_string(const char *str, vec2 pos, vec4 color, Baked_Font font, float line_height) {
    float x = pos[0];
    float y = pos[1];

    for (const char *cur = str; *cur != '\0'; cur++) {
        if ((uint8_t)*cur >= font.base_ascii && (uint8_t)*cur < (font.base_ascii + font.glyph_count))
        {
            stbtt_bakedchar metrics = font.glyph_metrics[*cur - font.base_ascii];

            float glyph_px_w = (float)(metrics.x1 - metrics.x0);
            float glyph_px_h = (float)(metrics.y1 - metrics.y0);

            Rect dest = {
                x + (float)metrics.xoff,
                y + (float)metrics.yoff,
                glyph_px_w,
                glyph_px_h
            };

            Rect src = {
                metrics.x0,
                metrics.y0,
                glyph_px_w,
                glyph_px_h
            };

            draw_texture(dest, font.tex, src, color);

            x += (float)metrics.xadvance;
        } else if (*cur == '\n') {
            x = pos[0];
            y += line_height;
        } else {
            exit_with_error("Non-printable glyph encountered: 0x%02X\n", *cur);
        }
    }
}

void draw_string_with_cursor(const char *str, size_t cursor, vec2 pos, vec4 color, Baked_Font font, float line_height) {
    float x = pos[0];
    float y = pos[1];

    // HACKY
    static int frame_counter = 0;
    static size_t prev_cursor = 0;
    frame_counter++;
    if (frame_counter >= 60) frame_counter = 0;

    if (cursor != prev_cursor) {
        frame_counter = 0;
        prev_cursor = cursor;
    }
    bool drew_cursor = false;
    bool will_draw_cursor =  !((frame_counter / 30) % 2);
    size_t current_index = 0;
    for (const char *cur = str; *cur != '\0'; cur++, current_index++) {
        if ((uint8_t)*cur >= font.base_ascii && (uint8_t)*cur < (font.base_ascii + font.glyph_count))
        {
            stbtt_bakedchar metrics = font.glyph_metrics[*cur - font.base_ascii];

            float glyph_px_w = (float)(metrics.x1 - metrics.x0);
            float glyph_px_h = (float)(metrics.y1 - metrics.y0);

            Rect dest = {
                x + (float)metrics.xoff,
                y + (float)metrics.yoff,
                glyph_px_w,
                glyph_px_h
            };

            Rect src = {
                metrics.x0,
                metrics.y0,
                glyph_px_w,
                glyph_px_h
            };

            if (!will_draw_cursor || current_index != cursor) {
                draw_texture(dest, font.tex, src, color);
            } else {
                Rect block_cursor = {
                    x,
                    y - line_height,
                    (float)metrics.xadvance,
                    line_height
                };
                draw_quad(block_cursor, color);
                vec4 inverted_color = (vec4){1.0f - color[0], 1.0f - color[1], 1.0f - color[2], color[3]};
                draw_texture(dest, font.tex, src, inverted_color);
                drew_cursor = true;
            }

            x += (float)metrics.xadvance;
        } else if (*cur == '\n') {
            if (will_draw_cursor && current_index == cursor) {
                Rect block_cursor = {
                    x,
                    y - line_height,
                    (float)10.0f,
                    line_height
                };
                draw_quad(block_cursor, color);
                drew_cursor = true;
            }

            x = pos[0];
            y += line_height;
        } else {
            trace_log("Non-printable glyph encountered: 0x%02X at %d\n", *cur, current_index);
        }
    }

    if (will_draw_cursor && !drew_cursor) {
        Rect block_cursor = {
            x,
            y - line_height,
            (float)10.0f,
            line_height
        };
        draw_quad(block_cursor, color);
    }
}

void handle_input_char(uint32_t c) {
    if (g_text_edit_state.used_size + 1 < ONE_MB) {
        // NOTE: The null terminator at used_size will be also copied forward
        size_t i = g_text_edit_state.used_size + 1;
        while (i > g_text_edit_state.text_buffer_cursor) {
            i--;
            g_text_edit_state.text_buffer[i + 1] = g_text_edit_state.text_buffer[i];
        }

        g_text_edit_state.text_buffer[g_text_edit_state.text_buffer_cursor++] = (char)c;
        g_text_edit_state.used_size++;
    } else {
        trace_log("Text buffer is full at %d bytes out of %d.", g_text_edit_state.used_size, ONE_MB);
    }
}

void handle_backspace_char() {
    if (g_text_edit_state.text_buffer_cursor > 0) {
        // NOTE: Include used_size as well, to move back the null terminator
        //       Note that it could be possible to delete more than one char at a time later on.
        //       So even if buffer is zero-initialized, not carrying the null terminator would be a problem.
        for (size_t i = g_text_edit_state.text_buffer_cursor; i <= g_text_edit_state.used_size; i++) {
            g_text_edit_state.text_buffer[i - 1] = g_text_edit_state.text_buffer[i];
        }

        g_text_edit_state.used_size--;
        g_text_edit_state.text_buffer_cursor--;
    }
}

void advance_cursor(bool forward) {
    if (forward) {
        g_text_edit_state.text_buffer_cursor++;
        if (g_text_edit_state.text_buffer_cursor > g_text_edit_state.used_size)
            g_text_edit_state.text_buffer_cursor = g_text_edit_state.used_size;
    } else if (g_text_edit_state.text_buffer_cursor > 0) {
        g_text_edit_state.text_buffer_cursor--;
    }
}

void save_file() {
    FILE *file = fopen(g_text_edit_state.file_name, "w+");
    if (!file) {
        exit_with_error("Not able to open file for saving: %s", g_text_edit_state.file_name);
    }
    fprintf(file, "%s", g_text_edit_state.text_buffer);
    fclose(file);
    g_text_edit_state.notify_frames = 30;
}

void try_load_file() {
    FILE *file = fopen(g_text_edit_state.file_name, "r");
    if (!file) {
        trace_log("File doesn't exist. Will create new file: %s.", g_text_edit_state.file_name);
        return;
    }

    size_t bytes_copied = fread(g_text_edit_state.text_buffer, 1, ONE_MB, file);
    g_text_edit_state.used_size = bytes_copied;

    trace_log("Read %d bytes from file: %s.", bytes_copied, g_text_edit_state.file_name);
}
