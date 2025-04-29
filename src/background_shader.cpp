// background_shader.cpp
#include "background_shader.h"
#include <iostream>

static const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 texCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(pos.xy, 0.0, 1.0);
    TexCoord = texCoord;
}
)";

static const char* fragmentShaderSource = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D backgroundTex;

void main() {
    FragColor = texture(backgroundTex, TexCoord);
}
)";

GLuint BackgroundShader::compileShader(GLenum type, const std::string &source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Shader compile error: " << log << std::endl;
    }

    return shader;
}

BackgroundShader::BackgroundShader() {
    // Compile shaders
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    // Link program
    programId = glCreateProgram();
    glAttachShader(programId, vert);
    glAttachShader(programId, frag);
    glLinkProgram(programId);

    glDeleteShader(vert);
    glDeleteShader(frag);

    // Create fullscreen quad
    float quadVertices[] = {
        // positions   // texCoords (flipped vertically)
        -1.0f, -1.0f,  0.0f, 1.0f,  // bottom-left  → now uses texcoord v = 1.0
         1.0f, -1.0f,  1.0f, 1.0f,  // bottom-right → now uses texcoord v = 1.0
        -1.0f,  1.0f,  0.0f, 0.0f,  // top-left     → now uses texcoord v = 0.0
         1.0f,  1.0f,  1.0f, 0.0f   // top-right    → now uses texcoord v = 0.0
    };
    
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void BackgroundShader::render(GLuint textureId, int width, int height) {
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, width, height);

    glUseProgram(programId);
    glBindVertexArray(quadVAO);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glUniform1i(glGetUniformLocation(programId, "backgroundTex"), 1);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glEnable(GL_DEPTH_TEST); // Re-enable for 3D rendering
}

BackgroundShader::~BackgroundShader() {
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteProgram(programId);
}
