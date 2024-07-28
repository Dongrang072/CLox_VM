#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)
#define IS_STRING(value) isObjType(value, OBJ_STRING)

//올바른 ObjString 포인터를 포함하리라 예상하는 Value를 인수로 받음
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_STRING,
} ObjType;

struct Obj {
    ObjType type;
    struct Obj *pNext;
};

struct ObjString { //첫 번째 필드를 Obj로 만들어서 모든 Obj가 공유하는 상태를 만듦
    Obj obj;
    int length;
    char *chars;
    uint32_t hash; // cash 고려 각 ObjString마다 자기 문자열의 해시 코드를 저장 하고 즉시 캐시함 O(n)
};

ObjString *takeString(char *chars, int length);

ObjString *copyString(const char *chars, int length);

void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif //CLOX_OBJECT_H
