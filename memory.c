#include <stdlib.h>
#include "vm.h"
#include "memory.h"

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, newSize); //항상 ptr의 값이 이전 주소와 같다고 볼 수는 없다
    if (result == NULL) exit(1);
    return result;
}

static void freeObject(Obj *object) {
    switch (object->type) {
        case OBJ_FUNCTION: {
            ObjFunction *function = (ObjFunction *) object;
            freeChunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }
        case OBJ_NATIVE:
            FREE(ObjNative, object);
            break;
        case OBJ_STRING: {
            ObjString *string = (ObjString *) object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
    }
}

void freeObjects() {
    Obj *Object = vm.objects;
    while (Object != NULL) {
        Obj *pNext = Object->pNext;
        freeObject(Object);
        Object = pNext;
    }
}

void initArray(Array *array, size_t type) {
    array->capacity = 0;
    array->count = 0;
    array->values = NULL;
    array->type = type;
}

void writeArray(Array *array, void *value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY_FOR_TYPE_SIZE(array->type, array->values, oldCapacity, array->capacity);
    }
    memcpy(&((uint8_t *) array->values)[array->type * array->count], value, array->type);
    array->count++;
}

void freeArray(Array *array) {
    FREE_ARRAY(uint8_t, array->values, array->capacity);
    initArray(array, array->type);
}

//memory fragmentation에 대해서 어떤 조치를 취할 것인가
