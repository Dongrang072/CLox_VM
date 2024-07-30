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
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;

    if (chunk->lines == NULL || chunk->currentLine == 0 || chunk->lines[chunk->currentLine - 1] != line) {
        if (chunk->linesCapacity < chunk->currentLine + 1) {
            int oldLinesCapacity = chunk->linesCapacity;
            chunk->linesCapacity = GROW_CAPACITY(oldLinesCapacity);
            chunk->lines = GROW_ARRAY(int, chunk->lines, oldLinesCapacity, chunk->linesCapacity);
        }

        chunk->lines[chunk->currentLine] = line;
        chunk->currentLine++;
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
    if (!chunk || !chunk->lines) return -1;

    int currentOffset = 0;
    for (int i = 0; i < chunk->currentLine; i++) {
        if (offset < currentOffset + chunk->lines[i]) {
            return chunk->lines[i];
        }
        currentOffset += chunk->lines[i];
    }
    return -1;
}



