#ifndef CLOX_VALUE_H
#define CLOX_VALUE_H

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ,
} ValueType;

typedef struct { //sizeof(type) +padding 4 bytes + sizeof(double) = 16 bytes
    ValueType type;
    union { //변수 타입중에 가장 비트 크기가 큰 타입을 기준으로
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

//type check for Value
#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type== VAL_NUMBER)
#define IS_OBJ(value) ((value).type == VAL_OBJ)

//get value in c type
#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJ(value) ((value).as.obj)

//promote macro
#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL         ((Value){VAL_NIL, {.number=0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number= value}})
#define OBJ_VAL(object) ((Value){VAL_OBJ, {.obj = (Obj*)object}})

typedef struct {
    int capacity;
    int count;
    Value *values;
} ValueArray;

bool valuesEqual(Value a, Value b);

void initValueArray(ValueArray *array);

void writeValueArray(ValueArray *array, Value value);

void freeValueArray(ValueArray *array);

void printValue(Value value);

#endif //CLOX_VALUE_H
