#pragma once
#include <cstddef>
#include <cstdint>

class StackAllocator {
public:
    explicit StackAllocator(size_t sizeBytes);
    ~StackAllocator();

    void* Allocate(size_t sizeBytes, size_t alignment = alignof(std::max_align_t));

    using Marker = size_t;
    Marker GetMarker() const { return m_offset; }
    void FreeToMarker(Marker marker);
    void Clear();

private:
    uint8_t* m_buffer;
    size_t m_capacity;
    size_t m_offset;
};