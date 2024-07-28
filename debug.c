#include <stdio.h>

#include "debug.h"
#include "value.h"

void disassembleChunk(Chunk *chunk, const char *name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;) { //명령어마다 크기가 제각각이라 인스트럭션에 넘김
        offset = disassembleInstruction(chunk, offset);
    }
}

static int simpleInstruction(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int byteInstruction(const char* name, Chunk* chunk, int offset){
    uint8_t slot = chunk->code[offset +1];
    printf("%-16s %4d\n", name, slot);
    return offset +2;
}

static int constantInstruction(const char *name, Chunk *chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-22s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

static int defineGlobalInstruction(const char* name, Chunk* chunk, int offset){
    uint8_t constant = chunk->code[offset +1];
    uint8_t isConst = chunk->code[offset +2];
    printf("%-16s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("' %s\n", isConst == OP_TRUE ? "const" : "let");
    return offset + 3;
}

//static int longConstantInstruction(const char *name, Chunk *chunk, int offset) {
//    uint8_t byte1 = chunk->code[offset + 1];
//    uint8_t byte2 = chunk->code[offset + 2];
//    uint8_t byte3 = chunk->code[offset + 3];
//    int constant = (byte1 << 16) | (byte2 << 8) | byte3;
//    printf("-16s %4d '", name, constant);
//    printValue(chunk->constants.values[constant]);
//    printf("'\n");
//    return offset + 4;
//}

int disassembleInstruction(Chunk *chunk, int offset) {
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("  | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }
    uint8_t instruction = chunk->code[offset];

    switch (instruction) {
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
//        case OP_CONSTANT_LONG:
//            return constantInstruction("OP_CONSTANT_LONG", chunk, offset);
        case OP_NIL:
            return simpleInstruction("OP_NIL", offset);
        case OP_TRUE:
            return simpleInstruction("OP_TRUE", offset);
        case OP_FALSE:
            return simpleInstruction("OP_FALSE", offset);
        case OP_POP:
            return simpleInstruction("OP_POP", offset);
        case OP_GET_LOCAL:
            return byteInstruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return byteInstruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL:
            return constantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_CONST_GLOBAL:
            return constantInstruction("OP_DEFINE_CONST_GLOBAL", chunk, offset);
        case OP_DEFINE_LET_GLOBAL:
            return constantInstruction("OP_DEFINE_LET_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:
            return constantInstruction("OP_SET_GLOBAL", chunk, offset);
        case OP_EQUAL:
            return simpleInstruction("OP_EQUAL", offset);
        case OP_GREATER:
            return simpleInstruction("OP_GREATER", offset);
        case OP_LESS:
            return simpleInstruction("OP_LESS", offset);
        case OP_ADD:
            return simpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", offset);
        case OP_NOT:
            return simpleInstruction("OP_NOT", offset);
        case OP_NEGATIVE:
            return simpleInstruction("OP_NEGATIVE", offset);
        case OP_PRINT:
            return simpleInstruction("OP_PRINT", offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        default:
            printf("unknow opcode %d\n", instruction);
            return offset + 1;
    }
}

