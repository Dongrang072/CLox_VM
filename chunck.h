#ifndef CLOX_CHUNCK_H
#define CLOX_CHUNCK_H

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NEGATIVE,
    OP_RETURN,
    OP_TERNARY_TRUE,
    OP_TERNARY_FALSE,
}OpCode;

typedef struct { //dynamic Array
    int count;
    int capacity;
    uint8_t* code;
    int* lines;
    ValueArray constants;
}Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);
#endif //CLOX_CHUNCK_H
