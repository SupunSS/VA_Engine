#include "PoolAllocator.h"
#include "../Assert.h"
#include <cstdlib>

PoolAllocator::PoolAllocator(size_t blockSize, size_t blockCount)
    : m_blockSize(blockSize < sizeof(FreeNode) ? sizeof(FreeNode) : blockSize),
      m_blockCount(blockCount) {

    m_buffer = static_cast<uint8_t*>(std::malloc(m_blockSize * m_blockCount));
    ENGINE_ASSERT(m_buffer != nullptr, "PoolAllocator failed to allocate backing buffer");

    // Thread the free list through every block
    m_freeList = reinterpret_cast<FreeNode*>(m_buffer);
    for (size_t i = 0; i < m_blockCount - 1; ++i) {
        FreeNode* node = reinterpret_cast<FreeNode*>(m_buffer + i * m_blockSize);
        node->next = reinterpret_cast<FreeNode*>(m_buffer + (i + 1) * m_blockSize);
    }
    reinterpret_cast<FreeNode*>(m_buffer + (m_blockCount - 1) * m_blockSize)->next = nullptr;
}

PoolAllocator::~PoolAllocator() {
    std::free(m_buffer);
}

void* PoolAllocator::Allocate() {
    ENGINE_ASSERT(m_freeList != nullptr, "PoolAllocator out of blocks");
    FreeNode* node = m_freeList;
    m_freeList = node->next;
    return node;
}

void PoolAllocator::Free(void* ptr) {
    FreeNode* node = static_cast<FreeNode*>(ptr);
    node->next = m_freeList;
    m_freeList = node;
}