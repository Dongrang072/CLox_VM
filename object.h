#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "common.h"
#include "value.h"
#include "chunk.h"
//함수는 일급 객체로 취급하기
#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_CLOSURE(value) isObjectType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define IS_STRING(value) isObjType(value, OBJ_STRING)
//올바른 ObjString 포인터를 포함하리라 예상하는 Value를 인수로 받음

#define AS_CLOSURE(value) ((ObjClosure *)AS_OBJ(value))
#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative *)AS_OBJ(value))->function)
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE,
} ObjType;

struct Obj {
    ObjType type;
    Obj *pNext;
};

typedef struct {
    Obj obj;
    int arity;
    int upValueCount;
    Chunk chunk;
    ObjString *name;
} ObjFunction;

typedef Value (*NativeFn)(int argCount, Value *args);

typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

struct ObjString {
    //첫 번째 필드를 Obj로 만들어서 모든 Obj가 공유하는 상태를 만듦
    Obj obj;
    int length;
    char *chars;
    uint32_t hash; // cash 고려 각 ObjString마다 자기 문자열의 해시 코드를 저장 하고 즉시 캐시함 O(n)
};

typedef struct ObjUpValue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpValue* pNext;
} ObjUpValue;

typedef struct {
    Obj obj;
    ObjFunction* function;
    ObjUpValue** upValues;
    int upValueCount;
}ObjClosure;

ObjFunction *newFunction();

ObjNative *newNative(NativeFn function);

ObjString *takeString(char *chars, int length);

ObjString *copyString(const char *chars, int length);

ObjUpValue *newUpValue(Value* slot);

ObjClosure *newClosure(ObjFunction *function);

void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

ObjString *objToString(Obj *obj);

#endif //CLOX_OBJECT_H
