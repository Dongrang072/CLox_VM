#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunck.h"
#include "value.h"

#define STACK_MAX 256

typedef struct {
    Chunk* chunk;
    uint8_t * ip; //(instruction pointer): 항상 현재 처리중인 명령어가 아니라 다음에 실행할 명령어를 가르킨다
    Value stack[STACK_MAX];
    Value* stackTop;
}VM;

typedef enum{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
}InterpretResult;

void initVM();
void freeVM();
InterpretResult interpret(Chunk* chunk);
void push(Value value);
Value pop();
#endif //CLOX_VM_H
