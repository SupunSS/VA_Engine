#pragma once
#include <cstddef>
#include <cstdint>

class PoolAllocator {
public:
    PoolAllocator(size_t blockSize, size_t blockCount);
    ~PoolAllocator();

    void* Allocate();
    void Free(void* ptr);

private:
    struct FreeNode { FreeNode* next; };

    uint8_t* m_buffer;
    size_t m_blockSize;
    size_t m_blockCount;
    FreeNode* m_freeList;
};