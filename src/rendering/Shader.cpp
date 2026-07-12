#include "Shader.h"
#include "../core/Log.h"
#include "../core/Assert.h"
#include <glad/glad.h>
#include <fstream>
#include <sstream>

std::string Shader::ReadFile(const std::string& path) {
    std::ifstream file(path);
    ENGINE_ASSERT(file.is_open(), "Failed to open shader file");
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

unsigned int Shader::Compile(unsigned int type, const std::string& source) {
    unsigned int shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        Log::Error("Shader compilation failed: {}", infoLog);
        ENGINE_ASSERT(false, "Shader compilation failed");
    }
    return shader;
}

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath) {
    unsigned int vertexShader = Compile(GL_VERTEX_SHADER, ReadFile(vertexPath));
    unsigned int fragmentShader = Compile(GL_FRAGMENT_SHADER, ReadFile(fragmentPath));

    m_programID = glCreateProgram();
    glAttachShader(m_programID, vertexShader);
    glAttachShader(m_programID, fragmentShader);
    glLinkProgram(m_programID);

    int success;
    glGetProgramiv(m_programID, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_programID, 512, nullptr, infoLog);
        Log::Error("Shader linking failed: {}", infoLog);
        ENGINE_ASSERT(false, "Shader linking failed");
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

Shader::~Shader() {
    glDeleteProgram(m_programID);
}

void Shader::Bind() const {
    glUseProgram(m_programID);
}