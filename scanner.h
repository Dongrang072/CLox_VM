#ifndef CLOX_SCANNER_H
#define CLOX_SCANNER_H

#include <stdio.h>
#include <string.h>
#include "common.h"

typedef enum {
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
    TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR, TOKEN_PERCENT, TOKEN_QUESTION,
    TOKEN_COLON,

    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,

    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,

    TOKEN_INTERPOLATION,

    TOKEN_AND, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE,
    TOKEN_FOR, TOKEN_FUN, TOKEN_IF, TOKEN_NIL, TOKEN_OR,
    TOKEN_PRINTLN, TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER, TOKEN_THIS,
    TOKEN_TRUE, TOKEN_CONST, TOKEN_LET, TOKEN_WHILE,
    TOKEN_SWITCH, TOKEN_CASE, TOKEN_DEFAULT,
    TOKEN_CONTINUE, TOKEN_BREAK,

    TOKEN_ERROR, TOKEN_EOF,
} TokenType;

typedef struct {
    TokenType type;
    const char *start;
    int length;
    int line;
} Token;

void initScanner(const char *source);

Token scanToken();

static bool isDigit(char c);

static bool isAlpha(char c);

static TokenType identifierType();

#endif //CLOX_SCANNER_H
