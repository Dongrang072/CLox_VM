#include <stdlib.h>
#include "vm.h"
#include "memory.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize){
    if(newSize ==0 ){
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if(result == NULL) exit(1);
    return result;
}

static void freeObject(Obj* pObject){
    switch (pObject->type) {
        case OBJ_STRING:{
            ObjString* pString = (ObjString*)pObject;
            FREE_ARRAY(char, pString->chars, pString->length +1);
            FREE(ObjString, pObject);
            break;
        }
    }
}

void freeObjects(){
    Obj* pObject = vm.objects;
    while(pObject != NULL){
        Obj* pNext = pObject->pNext;
        freeObject(pObject);
        pObject = pNext;
    }
}