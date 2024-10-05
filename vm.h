#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"
#include "object.h"
#include "value.h"
#include "table.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct { // framePointer, basePointer
    ObjFunction* function;
    uint8_t* ip;
    Value* slots; //함수가 사용할 수 있는 첫번째 슬롯에 위치한 vm의 스택을 가르킨다.
}CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    Chunk *chunk;
    uint8_t *ip; //(instruction pointer): 항상 현재 처리중인 명령어가 아니라 다음에 실행할 명령어를 가르킨다
    Value stack[STACK_MAX];
    Value *stackTop;
    Table globals;
    Table strings;
    Obj *objects;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;


void initVM();

void freeVM();

InterpretResult interpret(const char *source);

void push(Value value);

Value pop();

#endif //CLOX_VM_H
