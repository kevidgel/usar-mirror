#pragma once

#include <glad/glad.h>
#include <string>

class BackgroundShader {
public:
    BackgroundShader();
    ~BackgroundShader();

    // Renders the video background as fullscreen quad
    void render(GLuint textureId, int width, int height);

    void use() {
        glUseProgram(programId);
    }

private:
    GLuint programId;
    GLuint quadVAO;
    GLuint quadVBO;

    GLuint compileShader(GLenum type, const std::string &source);
};
