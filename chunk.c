#include "chunk.h"
#include "memory.h"
#include "vm.h"

void initChunk(Chunk *chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}

void writeChunk(Chunk *chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int addConstant(Chunk *chunk, Value value) {
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}

//void writeConstant(Chunk *chunk, Value value, int line) {
//    //chunk의 상수 배열에 value를 추가한 다음 상수를 로드하는 적절한 명령어> OP_CONSTANT_LONG
//    //max size < 255[256bytes] 0 -255 value Array
//    int index = addConstant(chunk, value);
//
//    if (index < 256) {
//        writeChunk(chunk, OP_CONSTANT, line);
//        writeChunk(chunk, index, line);
//    } else {
//        writeChunk(chunk, OP_CONSTANT_LONG, line);
//
//        writeChunk(chunk, (index >> 16) & 0xFF, line);
//        writeChunk(chunk, (index >> 8) & 0xFF, line);
//        writeChunk(chunk, index & 0xFF, line);
//    }
//
//}

void freeChunk(Chunk *chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

//int getLine(Chunk *chunk, int instructionIndex) {
//    int start = 0;
//    int end = chunk->lineCount - 1;
//
//    while (start <= end) {
//        int mid = (start + end) / 2;
//        LineStart *line = &chunk->lines[mid];
//
//        if (instructionIndex < line->start) {
//            end = mid - 1;
//        } else if (instructionIndex >= line->start + line->count) {
//            start = mid + 1;
//        } else {
//            return line->line;
//        }
//    }
//    return -1;
