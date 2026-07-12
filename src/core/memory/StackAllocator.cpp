#include "StackAllocator.h"
#include "../Assert.h"
#include <cstdlib>

StackAllocator::StackAllocator(size_t sizeBytes)
    : m_capacity(sizeBytes), m_offset(0) {
    m_buffer = static_cast<uint8_t*>(std::malloc(sizeBytes));
    ENGINE_ASSERT(m_buffer != nullptr, "StackAllocator failed to allocate backing buffer");
}

StackAllocator::~StackAllocator() {
    std::free(m_buffer);
}

void* StackAllocator::Allocate(size_t sizeBytes, size_t alignment) {
    size_t currentAddr = reinterpret_cast<size_t>(m_buffer + m_offset);
    size_t misalignment = currentAddr % alignment;
    size_t adjustment = misalignment == 0 ? 0 : (alignment - misalignment);

    size_t newOffset = m_offset + adjustment + sizeBytes;
    ENGINE_ASSERT(newOffset <= m_capacity, "StackAllocator out of memory");

    void* result = m_buffer + m_offset + adjustment;
    m_offset = newOffset;
    return result;
}

void StackAllocator::FreeToMarker(Marker marker) {
    ENGINE_ASSERT(marker <= m_offset, "Invalid marker: cannot free forward");
    m_offset = marker;
}

void StackAllocator::Clear() {
    m_offset = 0;
}