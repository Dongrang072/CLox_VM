#include <stdlib.h>
#include "vm.h"
#include "memory.h"

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}

static void freeObject(Obj *object) {
    switch (object->type) {
        case OBJ_STRING: {
            ObjString *string = (ObjString *) object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
    }
}

void freeObjects(){
    Obj* Object = vm.objects;
    while(Object != NULL){
        Obj* pNext = Object->pNext;
        freeObject(Object);
        Object = pNext;
    }
}

//memory fragmentation에 대해서 어떤 조치를 취할 것인가

