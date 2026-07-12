#pragma once
#include <string>

class Texture {
public:
    explicit Texture(const std::string& path);
    ~Texture();

    void Bind(unsigned int slot = 0) const;

    unsigned int GetID() const { return m_textureID; }

private:
    unsigned int m_textureID;
};