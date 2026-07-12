#pragma once
#include <string>
#include <glm/glm.hpp>

class Shader {
public:
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    void Bind() const;

    unsigned int GetID() const { return m_programID; }
    
    void SetMat4(const std::string& name, const glm::mat4& matrix) const;

private:
    unsigned int Compile(unsigned int type, const std::string& source);
    std::string ReadFile(const std::string& path);

    unsigned int m_programID;
};