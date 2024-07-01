#include <stdio.h>

#include "compiler.h"
#include "scanner.h"
#include "common.h"


void compile(const char *source) {
    initScanner(source);
    int line = -1;
    for (;;) {
        Token token = scanToken();
        if (token.line != line) {
            printf("%4d ,", token.line);
        } else {
            printf("   | ");
        }
        //lexeme은 원본 소스 문자열을 가리키고 마지막에 종결자가 없기 때문에 길이를 제한해야 한다
        printf("%2d '%.*s'\n", token.type, token.length, token.start);

        if(token.type == TOKEN_EOF) break;

    }
}