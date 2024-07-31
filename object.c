#include <stdio.h>
#include <string.h>

#include "table.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

static Obj *allocateObject(size_t size, ObjType type) { //주어진 크기(객체 자체의 크기가 아님을 주의)의 객체를 힙에 할당함
    //객체 생성에 필요한 추가 payload 필드용 공간 확보를 위함
    Obj *object = (Obj *) reallocate(NULL, 0, size);
    object->type = type;

    object->pNext = vm.objects;
    vm.objects = object;
    return object;
}

static ObjString *allocateString(char *chars, int length, uint32_t hash) { //OOP의 생성자와 유사한 기능을 한다
    ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    //ObjString *string = (ObjString*)allocateObject(sizeof(ObjString*), OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    tableSet(&vm.strings, string, NIL_VAL, false); //문자열 상수는 const가 아니므로 entry->isConst는 false
    return string;
}

static uint32_t hashString(const char *key, int length) { //FNV-1a algorithm
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t) key[i];
        hash *= 16777619;
    }
    return hash;
}

ObjString *takeString(char *chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString *interned = tableFindString(&vm.strings, chars, length, hash);

    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(chars, length, hash);
}

ObjString *copyString(const char *chars, int length) { //사용자가 전달할 문자의 소유권을 가져올 수 없다고 가정함
    uint32_t hash = hashString(chars, length);
    ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) return interned;

    char *heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, hash);
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    }
}

ObjString* objToString(Obj* obj) { //for string interpolation
    switch (obj->type) {
        case OBJ_STRING:
            return AS_STRING(OBJ_VAL(obj));
            //추후에 다른 객체 타입에 대해서도 추가
        default:
            return copyString("unknown", 7);
    }
}


