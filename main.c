#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "vm.h"

static void repl() {
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
        interpret(line);
    }
}

static char *readFile(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) { //시스템에 파일이 존재하지 않거나, 사용자가 파일에 액세스할 권한이 없을 경우, 혹은 경로가 틀린 경우
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }
    fseek(file, 0L, SEEK_END); //fseek()으로 파일 끝을 찾음
    size_t fileSize = ftell(file);//ftell()을 호출해서 파일 시작부에서 몇 바이트나 떨어져있는지 알아냄
    rewind(file);

    char *buffer = (char *) malloc(fileSize + 1); // + null byte
    if (buffer == NULL) { //메모리 부족일 경우
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) { // read fail
        fprintf(stderr, "Could not read file \"%s\".\n", path);
    }
    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

static void runFile(const char *path) {
    char *source = readFile(path);
    InterpretResult result = interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char *argv[]) {
    initVM();

    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        runFile(argv[1]);
    } else {
        fprintf(stderr, "Usage: clox [path]\n");
    }

    freeVM();
    return 0;
}

