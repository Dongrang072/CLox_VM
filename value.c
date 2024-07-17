#include <stdio.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "value.h"


void initValueArray(ValueArray *array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void writeValueArray(ValueArray *array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values,
                                   oldCapacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(ValueArray *array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

void printValue(Value value) {
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NIL:
            printf("nil");
            break;
        case VAL_NUMBER:
            printf("%g", AS_NUMBER(value));
            break;
        case VAL_OBJ:
            printObject(value);
            break;
    }
}

bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) { //Value 타입이 다르면 동등하지 않다.
        switch (a.type) { //패딩과 크기가 가변적인 공용체 필드 때문에 사용하지 않는 비트도 포함될 수 있음.
            case VAL_BOOL:
                return AS_BOOL(a) == AS_BOOL(b);
            case VAL_NIL:
                return true;
            case VAL_NUMBER:
                return AS_NUMBER(a) == AS_NUMBER(b);
            case VAL_OBJ: {
                ObjString *aString = AS_STRING(a);
                ObjString *bString = AS_STRING(b);
                return aString->length == bString->length &&
                       memcpy(aString->chars, bString->chars, aString->length) == 0;
            }
            default:
                return false;
        }
    }
}