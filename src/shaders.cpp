#include "shaders.h"
#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

std::string FragmentShaderCode = R"(
	#version 330 core
	in vec3 normal;
	in vec3 position;
	in vec2 texcoord;
	
	uniform sampler2D tex;
	uniform vec3 sun_position;
	uniform vec3 sun_color;
	uniform float opacity;  // NEW: uniform to control transparency
	
	out vec4 color;
	
	void main() {
		float lum = max(dot(normal, normalize(sun_position)), 0.0);
		vec4 texColor = texture(tex, texcoord);
	
		// Optional discard for alpha clipping (can comment this if always blending)
		if (texColor.a < 0.1)
			discard;
	
		float alpha = texColor.a * opacity;  // Combine texture alpha and uniform opacity
		color = vec4(texColor.rgb * (0.3 + 0.7 * lum) * sun_color, alpha);
	}
	)";
	

std::string VertexShaderCode = R"(
#version 330 core
layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_texcoord;

uniform mat4 MVP;

out vec3 normal;
out vec3 position;
out vec2 texcoord;

void main() {
    gl_Position = MVP * vec4(in_vertex, 1.0);
    position = in_vertex;
    normal = normalize(mat3(MVP) * in_normal);
    texcoord = in_texcoord;
}
)";

Shaders::Shaders() {
    GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

    GLint Result = GL_FALSE;
    int InfoLogLength;

    // Compile Vertex Shader
    const char* VertexSourcePointer = VertexShaderCode.c_str();
    glShaderSource(VertexShaderID, 1, &VertexSourcePointer, NULL);
    glCompileShader(VertexShaderID);
    glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
    if (Result == GL_FALSE) {
        glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
        std::vector<char> ErrorMessage(InfoLogLength + 1);
        glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, ErrorMessage.data());
        std::cerr << "Vertex Shader Error:\n" << ErrorMessage.data() << std::endl;
    }

    // Compile Fragment Shader
    const char* FragmentSourcePointer = FragmentShaderCode.c_str();
    glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer, NULL);
    glCompileShader(FragmentShaderID);
    glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
    if (Result == GL_FALSE) {
        glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
        std::vector<char> ErrorMessage(InfoLogLength + 1);
        glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, ErrorMessage.data());
        std::cerr << "Fragment Shader Error:\n" << ErrorMessage.data() << std::endl;
    }

    // Link the program
    pid = glCreateProgram();
    glAttachShader(pid, VertexShaderID);
    glAttachShader(pid, FragmentShaderID);
    glLinkProgram(pid);

    // Check program
    glGetProgramiv(pid, GL_LINK_STATUS, &Result);
    if (Result == GL_FALSE) {
        glGetProgramiv(pid, GL_INFO_LOG_LENGTH, &InfoLogLength);
        std::vector<char> ErrorMessage(InfoLogLength + 1);
        glGetProgramInfoLog(pid, InfoLogLength, NULL, ErrorMessage.data());
        std::cerr << "Program Linking Error:\n" << ErrorMessage.data() << std::endl;
    }

    // Cleanup
    glDetachShader(pid, VertexShaderID);
    glDetachShader(pid, FragmentShaderID);
    glDeleteShader(VertexShaderID);
    glDeleteShader(FragmentShaderID);
}

Shaders::~Shaders() {}
