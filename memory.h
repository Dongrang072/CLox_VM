#ifndef CLOX_MEMORY_H
#define CLOX_MEMORY_H

#include "common.h"
#include "object.h"

typedef struct {
    int capacity;
    int count;
    void *values;
    size_t type;
} Array;

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCapacity, newCapacity) \
    (type*)reallocate((pointer), sizeof(type) * (oldCapacity), sizeof(type) * (newCapacity))

#define GROW_ARRAY_FOR_TYPE_SIZE(typeSize, pointer, oldCapacity, newCapacity) \
    reallocate((pointer), (typeSize) * (oldCapacity), sizeof(typeSize) * (newCapacity))

#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

#define READ_AS(type, array, i) (((type*) (array)->values)[i])

void *reallocate(void *pointer, size_t oldSize, size_t newSize);

void freeObjects();

void initArray(Array *array, size_t size);

void writeArray(Array *array, void* value);

void freeArray(Array *array);

#endif //CLOX_MEMORY_H
