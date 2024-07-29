#include "chunk.h"
#include "memory.h"


void initChunk(Chunk *chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    chunk->currentLine = 0;
    chunk->linesCapacity = 0;
    initValueArray(&chunk->constants);
}

void writeChunk(Chunk *chunk, uint8_t byte, int line) {
    if (chunk->count + 1 > chunk->capacity) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }

    if (chunk->currentLine + 2 >= chunk->linesCapacity) {
        int oldCapacity = chunk->linesCapacity;
        chunk->linesCapacity = GROW_CAPACITY(oldCapacity);
        chunk->lines = GROW_ARRAY(int , chunk->lines, oldCapacity, chunk->linesCapacity);
        ZERO_INITIALIZE(int , chunk->lines + oldCapacity, oldCapacity, chunk->linesCapacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;

    if (chunk->currentLine == 0 || line != chunk->lines[chunk->currentLine - 2]) {
        chunk->currentLine += 2;
        chunk->lines[chunk->currentLine - 2] = line;
        chunk->lines[chunk->currentLine - 1] = 1;
    } else {
        chunk->lines[chunk->currentLine - 1]++;
    }

}

void undoLastByte(Chunk *chunk) {
    if (chunk->count > 0) {
        chunk->count--;
    }
}

int addConstant(Chunk *chunk, Value value) {
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}


void freeChunk(Chunk *chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->linesCapacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

int writeConstant(Chunk *chunk, Value value, int line) { //The maximum number of constants is 2^24
    //chunk의 상수 배열에 value를 추가한 다음 상수를 로드하는 적절한 명령어> OP_CONSTANT_LONG

    writeChunk(chunk, OP_CONSTANT_LONG, line);
    int constIndex = addConstant(chunk, value);
    /* write the index of the constant as a 24 bit integer*/
    uint8_t index1 = (constIndex >> 16) &0xff;
    uint8_t index2 = (constIndex >> 8) & 0xff;
    uint8_t index3 = constIndex & 0xff;

    writeChunk(chunk, index1, line);
    writeChunk(chunk, index2, line);
    writeChunk(chunk, index3, line);

    return constIndex;
}

int getLineNumber(Chunk *chunk, int offset) {
    if (offset < 0 || offset >= chunk->count) {
        return -1; // invalid index
    }

    int i = 0;
    int tally = chunk->lines[1];

    while (offset + 1 > tally) {
        i += 2;
        tally += chunk->lines[i - 1];
    }
    return chunk->lines[i - 2];
}



