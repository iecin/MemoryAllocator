// PRIxPTR.
#include <inttypes.h>
// malloc, calloc, realloc, free.
#include <stdlib.h>
// EOF, FILE, stdin, printf.
#include <stdio.h>

#include "MemoryAllocator.h"

#define TRACK_MEMORY

#define UNREACHABLE                                                                  \
  fprintf(stderr, "In \"%s\", line %d\n Error: Unreachable.\n", __FILE__, __LINE__); \
  exit(EXIT_FAILURE);

#ifdef TRACK_MEMORY
// The minimum value have to be higher than 4 to avoid infinity lookup. Lower value is slower.
#define _INITIAL_CAPACITY 4
// The maximum value is 0.75. Lower value is faster.
#define _MAX_LOAD 0.5
// The value have to be the power of two. WARNING: Do not change.
#define _GROW_FACTOR 2

typedef struct MemoryAllocatorEntry {
  void* pointer;
  size_t size;
  char* sourceName;
  uint32_t lineNumber;
} MemoryAllocatorEntry;
#endif

typedef struct MemoryAllocator {
  size_t memoryUsage;
#ifdef TRACK_MEMORY
  size_t peakMemoryUsage;
  uint32_t entriesCapacity;
  uint32_t length;
  MemoryAllocatorEntry** entries;
#endif
} MemoryAllocator;

#ifdef TRACK_MEMORY
static const uint32_t _hash(const void* pointer) {
  uint64_t hash64 = (uint64_t) (uintptr_t) pointer;
  // 64-bit to 32-bit Hash Function. Read more: https://gist.github.com/badboy/6267743.
  hash64 = ~hash64 + (hash64 << 18);
  hash64 = hash64 ^ (hash64 >> 31);
  hash64 = hash64 * 21;
  hash64 = hash64 ^ (hash64 >> 11);
  hash64 = hash64 + (hash64 << 6);
  hash64 = hash64 ^ (hash64 >> 22);
  const uint32_t hash = (uint32_t) (hash64 & 0x3fffffff);
  return hash;
}
#endif

MemoryAllocator* MemoryAllocator_create() {
  MemoryAllocator* memoryAllocator = (MemoryAllocator*) calloc(1, sizeof(MemoryAllocator));
  if (!memoryAllocator) {
    fprintf(stderr, "SL Error: Out of memory.\n");
    exit(EXIT_FAILURE);
  }
  memoryAllocator->memoryUsage = sizeof(MemoryAllocator);
#ifdef TRACK_MEMORY
  memoryAllocator->peakMemoryUsage = 0;
  memoryAllocator->entriesCapacity = _INITIAL_CAPACITY;
  memoryAllocator->length = 0;
  memoryAllocator->entries = (MemoryAllocatorEntry**) calloc(memoryAllocator->entriesCapacity, sizeof(MemoryAllocatorEntry*));
  for (uint32_t i = 0; i < memoryAllocator->entriesCapacity; i++) {
    memoryAllocator->entries[i] = NULL;
  }
#endif
  return memoryAllocator;
}

void MemoryAllocator_delete(MemoryAllocator* memoryAllocator) {
  memoryAllocator->memoryUsage -= sizeof(MemoryAllocator);
  if (memoryAllocator->memoryUsage) {
    // fprintf(stderr, "SL Error: Memory leak.\n");
    MemoryAllocator_print(memoryAllocator);
    // exit(EXIT_FAILURE);
  }
#ifdef TRACK_MEMORY
  for (uint32_t i = 0; i < memoryAllocator->entriesCapacity; i++) {
    MemoryAllocatorEntry* entry = memoryAllocator->entries[i];
    if (entry) {
      free(entry->pointer);
      free(entry->sourceName);
      free(entry);
    }
  }
  free(memoryAllocator->entries);
#endif
  free(memoryAllocator);
}

const size_t MemoryAllocator_getMemoryUsage(const MemoryAllocator* memoryAllocator) {
  return memoryAllocator->memoryUsage;
}

#ifdef TRACK_MEMORY
#define _CALCULATE_INDEX(_index, _capacity) _index&(_capacity - 1)

static void _MemoryAllocator_rehash(MemoryAllocator* memoryAllocator) {
  const uint32_t capacity = memoryAllocator->entriesCapacity;
  memoryAllocator->entriesCapacity *= _GROW_FACTOR;
  MemoryAllocatorEntry** newEntries = (MemoryAllocatorEntry**) calloc(memoryAllocator->entriesCapacity, sizeof(MemoryAllocatorEntry*));
  if (!newEntries) {
    fprintf(stderr, "SL Error: Out of memory.\n");
    exit(EXIT_FAILURE);
  }
  for (uint32_t i = 0; i < memoryAllocator->entriesCapacity; i++) {
    newEntries[i] = NULL;
  }
  uint32_t i = capacity;
  while (i--) {
    MemoryAllocatorEntry* newEntry = memoryAllocator->entries[i];
    if (newEntry) {
      const uint32_t startIndex = _CALCULATE_INDEX(_hash(newEntry->pointer), memoryAllocator->entriesCapacity);
      uint32_t index = startIndex;
      for (;;) {
        if (!newEntries[index]) {
          break;
        }
        index = _CALCULATE_INDEX((index + 1), memoryAllocator->entriesCapacity);
        if (index == startIndex) {
          UNREACHABLE
        }
      }
      newEntries[index] = newEntry;
    }
  }
  free(memoryAllocator->entries);
  memoryAllocator->entries = newEntries;
}

static void _MemoryAllocator_addEntry(MemoryAllocator* memoryAllocator, void* pointer, const size_t size, const char* sourceName, const uint32_t lineNumber) {
  if (memoryAllocator->memoryUsage > memoryAllocator->peakMemoryUsage) {
    memoryAllocator->peakMemoryUsage = memoryAllocator->memoryUsage;
  }
  if (memoryAllocator->length >= _MAX_LOAD * memoryAllocator->entriesCapacity) {
    _MemoryAllocator_rehash(memoryAllocator);
  }
  const uint32_t startIndex = _CALCULATE_INDEX(_hash(pointer), memoryAllocator->entriesCapacity);
  uint32_t index = startIndex;
  for (;;) {
    MemoryAllocatorEntry* entry = memoryAllocator->entries[index];
    if (!entry) {
      entry = (MemoryAllocatorEntry*) calloc(1, sizeof(MemoryAllocatorEntry));
      if (!entry) {
        fprintf(stderr, "SL Error: Out of memory.\n");
        exit(EXIT_FAILURE);
      }
      // For debugging only.
      // printf("Allocated pointer %" PRIxPTR ".\n", (uintptr_t) pointer);
      entry->pointer = pointer;
      entry->size = size;
      entry->sourceName = strdup(sourceName);
      if (!entry->sourceName) {
        fprintf(stderr, "SL Error: Out of memory.\n");
        exit(EXIT_FAILURE);
      }
      entry->lineNumber = lineNumber;
      memoryAllocator->entries[index] = entry;
      memoryAllocator->length++;
      return;
    } else if (entry->pointer == pointer) {
      UNREACHABLE
    }
    index = _CALCULATE_INDEX((index + 1), memoryAllocator->entriesCapacity);
    if (index == startIndex) {
      UNREACHABLE
    }
  }
  UNREACHABLE
}
#endif

void* MemoryAllocator_allocate(MemoryAllocator* memoryAllocator, const size_t size, const char* sourceName, const uint32_t lineNumber) {
  memoryAllocator->memoryUsage += size;
  void* pointer = calloc(1, size);
  if (!pointer) {
    fprintf(stderr, "SL Error: Out of memory.\n");
    exit(EXIT_FAILURE);
  }
#ifdef TRACK_MEMORY
  _MemoryAllocator_addEntry(memoryAllocator, pointer, size, sourceName, lineNumber);
#endif
  return pointer;
}

void* MemoryAllocator_reallocate(MemoryAllocator* memoryAllocator, void* pointer, const size_t size, const size_t newSize, const char* sourceName, const uint32_t lineNumber) {
  memoryAllocator->memoryUsage += newSize - size;
  void* newPointer = realloc(pointer, newSize);
  if (!newPointer) {
    fprintf(stderr, "SL Error: Out of memory.\n");
    exit(EXIT_FAILURE);
  }
#ifdef TRACK_MEMORY
  if (memoryAllocator->memoryUsage > memoryAllocator->peakMemoryUsage) {
    memoryAllocator->peakMemoryUsage = memoryAllocator->memoryUsage;
  }
  const uint32_t startIndex = _CALCULATE_INDEX(_hash(pointer), memoryAllocator->entriesCapacity);
  uint32_t index = startIndex;
  for (;;) {
    MemoryAllocatorEntry* entry = memoryAllocator->entries[index];
    if (entry && entry->pointer == pointer) {
      if (size != entry->size) {
        fprintf(stderr, "In %s, line %d.\n  SL Internal Error: Invalid reallocation size. %zu is reallocated as %zu.\n", sourceName, lineNumber, entry->size, size);
        fprintf(stderr, "Allocated in %s, line %d.\n", entry->sourceName, entry->lineNumber);
        exit(EXIT_FAILURE);
      }
      // The reallocation may have moved the pointer.
      // For debugging only.
      // if (pointer != newPointer) {
      //   printf("Moved pointer %" PRIxPTR " to %" PRIxPTR "\n", (uintptr_t) pointer, (uintptr_t) newPointer);
      // }
      free(entry->sourceName);
      free(entry);
      memoryAllocator->length--;
      memoryAllocator->entries[index] = NULL;
      break;
    }
    index = _CALCULATE_INDEX((index + 1), memoryAllocator->entriesCapacity);
    if (index == startIndex) {
      fprintf(stderr, "In %s, line %d.\n  SL Internal Error: Invalid reallocation address %" PRIxPTR ".\n", sourceName, lineNumber, (uintptr_t) pointer);
      exit(EXIT_FAILURE);
    }
  }
  _MemoryAllocator_addEntry(memoryAllocator, newPointer, newSize, sourceName, lineNumber);
#endif
  return newPointer;
}

void MemoryAllocator_deallocate(MemoryAllocator* memoryAllocator, void* pointer, const size_t size, const char* sourceName, const uint32_t lineNumber) {
  memoryAllocator->memoryUsage -= size;
  free(pointer);
#ifdef TRACK_MEMORY
  const uint32_t startIndex = _CALCULATE_INDEX(_hash(pointer), memoryAllocator->entriesCapacity);
  uint32_t index = startIndex;
  for (;;) {
    MemoryAllocatorEntry* entry = memoryAllocator->entries[index];
    if (entry && entry->pointer == pointer) {
      if (size != entry->size) {
        fprintf(stderr, "In %s, line %d.\n  SL Internal Error: Invalid deallocation size. %zu is deallocated as %zu.\n", sourceName, lineNumber, size, entry->size);
        fprintf(stderr, "Allocated in %s, line %d.\n", entry->sourceName, entry->lineNumber);
        exit(EXIT_FAILURE);
      }
      // For debugging only.
      // printf("Deallocated pointer %" PRIxPTR ".\n", (uintptr_t) pointer);
      free(entry->sourceName);
      free(entry);
      memoryAllocator->length--;
      memoryAllocator->entries[index] = NULL;
      return;
    }
    index = _CALCULATE_INDEX((index + 1), memoryAllocator->entriesCapacity);
    if (index == startIndex) {
      fprintf(stderr, "In %s, line %d.\n  SL Internal Error: Invalid deallocation address %" PRIxPTR ".\n", sourceName, lineNumber, (uintptr_t) pointer);
      exit(EXIT_FAILURE);
    }
  }
#endif
}

#ifdef TRACK_MEMORY
#undef _CALCULATE_INDEX
#endif

void MemoryAllocator_print(const MemoryAllocator* memoryAllocator) {
#ifdef TRACK_MEMORY
  printf("Memory\n");
  printf("  Memory Usage: %zu bytes.\n", memoryAllocator->memoryUsage);
  printf("  Peak Memory Usage: %zu bytes.\n", memoryAllocator->peakMemoryUsage);
  if (memoryAllocator->length) {
    printf("Blocks\n");
    for (uint32_t i = 0; i < memoryAllocator->entriesCapacity; i++) {
      const MemoryAllocatorEntry* entry = memoryAllocator->entries[i];
      if (entry) {
        printf("  %d. Memory 0x%.8" PRIxPTR " of size %zu. Allocated in \"%s\", line %d.\n", i + 1, (uintptr_t) entry->pointer, entry->size, entry->sourceName, entry->lineNumber);
      }
    }
  }
#else
  printf("Memory Usage: %d bytes.\n", memoryAllocator->memoryUsage);
#endif
}