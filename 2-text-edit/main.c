#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "glad/glad.h"
#include <GLFW/glfw3.h>

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
    static float tri_vertices[] = {
	-0.5f, -0.5f, 0.0f,
	0.5f, -0.5f, 0.0f,
	0.0f,  0.5f, 0.0f
    };

    static const char *vert_shader_source =
	"#version 430 core\n"
	"layout (location = 0) in vec3 aPos;\n"
	"void main() {\n"
	"    gl_Position = vec4(aPos, 1.0);\n"
	"}";

    static const char *frag_shader_source =
	"#version 430 core\n"
	"out vec4 FragColor;\n"
	"void main() {\n"
	"    FragColor = vec4(1.0, 0.5, 0.2, 1.0);\n"
	"}";

    uint32_t vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tri_vertices), tri_vertices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *) 0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

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

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindVertexArray(0);
}
