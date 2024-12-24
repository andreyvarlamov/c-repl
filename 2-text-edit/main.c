#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "glad/glad.h"
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

static uint32_t texture;
static uint32_t vao;
static uint32_t shader_program_id;

void exit_with_error(const char *msg);
void trace_log(const char *msg, ...);

void keyboard_callback(GLFWwindow *window, int key, int scancode, int action, int mods);

void initialize_gl();
void draw();

int main() {
    if (!glfwInit()) {
	exit_with_error("Failed to initialize GLFW");
    }

    trace_log("GLFW initialized");

    // Set GLFW OpenGL version -- 4.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* glfw_window = glfwCreateWindow(800, 600, "Text Edit Proto", NULL, NULL);
    if (glfw_window == NULL) {
	exit_with_error("Failed to create GLFW window");
    }

    trace_log("GLFW window created");

    glfwMakeContextCurrent(glfw_window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
	exit_with_error("Failed to load GL function pointers");
    }

    trace_log("Loaded OpenGL function pointers. Debug info:");
    trace_log("  Version:  %s", glGetString(GL_VERSION));
    trace_log("  GLSL:     %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
    trace_log("  Vendor:   %s", glGetString(GL_VENDOR));
    trace_log("  Renderer: %s", glGetString(GL_RENDERER));

    glfwSetKeyCallback(glfw_window, keyboard_callback);

    initialize_gl();
    
    trace_log("Entering main loop");

    while (!glfwWindowShouldClose(glfw_window)) {
	draw();

	glfwSwapBuffers(glfw_window);
	glfwPollEvents();
    }

    trace_log("GLFW terminating gracefully");

    glfwTerminate();
    return 0;
}

void exit_with_error(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
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

void keyboard_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    (void)window; (void)key; (void)scancode; (void)action; (void)mods;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
	trace_log("Received ESC. Terminating...");
	glfwSetWindowShouldClose(window, true);
    }
}

void initialize_gl() {
    static float quad_vertices[] = {
	// Positions         // Texture Coords
	-0.5f, -0.5f, 0.0f,  0.0f, 0.0f,  // Bottom-left
	 0.5f, -0.5f, 0.0f,  1.0f, 0.0f,  // Bottom-right
	 0.5f,  0.5f, 0.0f,  1.0f, 1.0f,  // Top-right
	-0.5f,  0.5f, 0.0f,  0.0f, 1.0f   // Top-left
    };

    static uint32_t quad_indices[] = {
	0, 1, 2,  // First Triangle
	0, 2, 3   // Second Triangle
    };

    static const char *vert_shader_source =
	"#version 430 core\n"
	"layout (location = 0) in vec3 aPos;\n"
	"layout (location = 1) in vec2 aTexCoord;\n"
	"out vec2 TexCoord;\n"
	"void main() {\n"
	"    gl_Position = vec4(aPos, 1.0);\n"
	"    TexCoord = aTexCoord;\n"
	"}";

    static const char *frag_shader_source =
	"#version 430 core\n"
	"out vec4 FragColor;\n"
	"in vec2 TexCoord;\n"
	"uniform sampler2D texture1;\n"
	"void main() {\n"
	"    FragColor = texture(texture1, TexCoord);\n"
	"}";

    uint32_t vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_indices), quad_indices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) 0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) (3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(true);
    int width, height, channel_count;
    uint8_t *image_data = stbi_load("res/claesz.png", &width, &height, &channel_count, STBI_rgb_alpha);
    if (image_data == NULL) {
	exit_with_error("Failed to load image at res/claesz.png");
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(image_data);

    uint32_t vert_shader_id = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_shader_id, 1, &vert_shader_source, NULL);
    glCompileShader(vert_shader_id);

    uint32_t frag_shader_id = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_shader_id, 1, &frag_shader_source, NULL);
    glCompileShader(frag_shader_id);

    shader_program_id = glCreateProgram();
    glAttachShader(shader_program_id, vert_shader_id);
    glAttachShader(shader_program_id, frag_shader_id);
    glLinkProgram(shader_program_id);

    glDeleteShader(vert_shader_id);
    glDeleteShader(frag_shader_id);
}

void draw() {
    glUseProgram(shader_program_id);
    glBindVertexArray(vao);
    glBindTexture(GL_TEXTURE_2D, texture);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}
