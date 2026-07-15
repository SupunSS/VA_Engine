#include "Shader.h"
#include "core/Log.h"
#include "core/Assert.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
{
    std::string vertexSource = ReadFile(vertexPath);
    std::string fragmentSource = ReadFile(fragmentPath);

    unsigned int vertexShader = Compile(GL_VERTEX_SHADER, vertexSource);
    unsigned int fragmentShader = Compile(GL_FRAGMENT_SHADER, fragmentSource);

    m_programID = glCreateProgram();
    glAttachShader(m_programID, vertexShader);
    glAttachShader(m_programID, fragmentShader);
    glLinkProgram(m_programID);

    int success;
    glGetProgramiv(m_programID, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_programID, 512, nullptr, infoLog);
        Log::Info("Shader linking failed: {}", infoLog);
        ENGINE_ASSERT(false, "Shader program failed to link");
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

Shader::~Shader()
{
    glDeleteProgram(m_programID);
}

void Shader::Bind() const
{
    glUseProgram(m_programID);
}

void Shader::SetMat4(const std::string& name, const glm::mat4& matrix) const
{
    int location = glGetUniformLocation(m_programID, name.c_str());
    glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
}

void Shader::SetVec3(const std::string& name, const glm::vec3& value) const
{
    int location = glGetUniformLocation(m_programID, name.c_str());
    glUniform3fv(location, 1, glm::value_ptr(value));
}

void Shader::SetInt(const std::string& name, int value) const
{
    int location = glGetUniformLocation(m_programID, name.c_str());
    glUniform1i(location, value);
}

void Shader::SetFloat(const std::string& name, float value) const
{
    int location = glGetUniformLocation(m_programID, name.c_str());
    glUniform1f(location, value);
}

unsigned int Shader::Compile(unsigned int type, const std::string& source)
{
    unsigned int shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        Log::Info("Shader compilation failed ({}): {}",
            type == GL_VERTEX_SHADER ? "vertex" : "fragment", infoLog);
        ENGINE_ASSERT(false, "Shader failed to compile");
    }

    return shader;
}

std::string Shader::ReadFile(const std::string& path)
{
    std::ifstream file(path);
    ENGINE_ASSERT(file.is_open(), "Failed to open shader file: " + path);

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

