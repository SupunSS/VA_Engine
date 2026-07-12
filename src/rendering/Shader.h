#pragma once
#include <string>

class Shader {
public:
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    void Bind() const;

    unsigned int GetID() const { return m_programID; }

private:
    unsigned int Compile(unsigned int type, const std::string& source);
    std::string ReadFile(const std::string& path);

    unsigned int m_programID;
};