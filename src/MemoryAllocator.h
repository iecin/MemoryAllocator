#ifndef MEMORY_ALLOCATOR
#define MEMORY_ALLOCATOR

// int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t, uintptr_t.
#include <stdint.h>

typedef struct MemoryAllocator MemoryAllocator;

#define ALLOCATE(_memoryAllocator, _size) MemoryAllocator_allocate(_memoryAllocator, _size, __FILE__, __LINE__)
#define REALLOCATE(_memoryAllocator, _pointer, _oldSize, _newSize) MemoryAllocator_reallocate(_memoryAllocator, _pointer, _oldSize, _newSize, __FILE__, __LINE__)
#define DEALLOCATE(_memoryAllocator, _pointer, _size) MemoryAllocator_deallocate(_memoryAllocator, _pointer, _size, __FILE__, __LINE__)

MemoryAllocator* MemoryAllocator_create();
void MemoryAllocator_delete(MemoryAllocator*);
const size_t MemoryAllocator_getMemoryUsage(const MemoryAllocator*);
void* MemoryAllocator_allocate(MemoryAllocator*, const size_t, const char*, const uint32_t);
void* MemoryAllocator_reallocate(MemoryAllocator*, void*, const size_t, const size_t, const char*, const uint32_t);
void MemoryAllocator_deallocate(MemoryAllocator*, void*, const size_t, const char*, const uint32_t);
void MemoryAllocator_print(const MemoryAllocator*);

#endif