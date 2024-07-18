#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"
#include "scanner.h"
#include "common.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    Token current;
    Token previous;
    bool hadError; //compile error flag
    bool panicMode;
} Parser;

typedef enum { //우선순위에 따라 크고 작음을 다룸
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_TERNARY, // ? ternary()
    PREC_OR,         //or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! -
    PREC_CALL,       // . ()
    PREC_PRIMARY
} Precedence;

typedef void(*ParseFn)();

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

Parser parser;
Chunk *compilingChunk;

static Chunk *currentChunk() {
    return compilingChunk;
}

static void errorAt(Token *token, const char *message) {
    if (parser.panicMode) return; // panic mode flag가 세팅되면 다른 에러가 나도 그냥 무시
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line); //error가 발생한 줄 번호 출력

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, "at end");
    } else if (token->type == TOKEN_ERROR) {
        //none
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }
    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char *message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char *message) {
    errorAt(&parser.current, message);
}

static void advance() {
    parser.previous = parser.current;
    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char *message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

static void emitByte(uint8_t byte) { //chunk에 1바이트 추가
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitReturn() {
    emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) { //OP_CONSTANT 명령어는 인덱스 피연산자로 1바이트를 사용하므로 최대 256개 상수까지 저장/로드 가능
        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint8_t) constant;
}

static void emitConstant(Value value) { //상수 테이블에 엔트리 추가
    emitBytes(OP_CONSTANT, makeConstant(value));
}

static void endCompiler() {
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

static void expression();

static ParseRule *getRule(TokenType type);

static void parsePrecedence(Precedence precedence);

static void binary() {
    TokenType operatorType = parser.previous.type;
    ParseRule *rule = getRule(operatorType);
    parsePrecedence((Precedence) (rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL:
            emitBytes(OP_EQUAL, OP_NOT);
            break;
        case TOKEN_EQUAL_EQUAL:
            emitByte(OP_EQUAL);
            break;
        case TOKEN_GREATER:
            emitByte(OP_GREATER);
            break;
        case TOKEN_GREATER_EQUAL:
            emitBytes(OP_LESS, OP_NOT);
            break;
        case TOKEN_LESS:
            emitByte(OP_LESS);
            break;
        case TOKEN_LESS_EQUAL:
            emitBytes(OP_GREATER, OP_NOT);
            break;
        case TOKEN_PLUS:
            emitByte(OP_ADD);
            break;
        case TOKEN_MINUS:
            emitByte(OP_SUBTRACT);
            break;
        case TOKEN_STAR:
            emitByte(OP_MULTIPLY);
            break;
        case TOKEN_SLASH:
            emitByte(OP_DIVIDE);
            break;
        default:
            return;
    }
}

static void literal() {
    switch (parser.previous.type) {
        case TOKEN_FALSE:
            emitByte(OP_FALSE);
            break;
        case TOKEN_NIL:
            emitByte(OP_NIL);
            break;
        case TOKEN_TRUE:
            emitByte(OP_TRUE);
            break;
        default:
            return; //실행되지 않은 코드
    }
}

static void grouping() {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

static void number() {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void string(){
    emitConstant(OBJ_VAL(copyString(parser.previous.start +1, parser.previous.length-2))); // 전후 " 제거
}

//static void ternary() {
//    // 현재는 `?` 토큰을 이미 소비한 상태
//    // 조건 부분은 이미 파싱되었고 스택에 남아있음
//    parsePrecedence(PREC_TERNARY);
//    consume(TOKEN_COLON, "Expect ':' after then branch of conditional expression.");
//
//    parsePrecedence(PREC_ASSIGNMENT);
//
//    emitByte(OP_TERNARY_TRUE);
//    emitByte(OP_TERNARY_FALSE);
//}

static void unary() {
    TokenType operatorType = parser.previous.type;
    //expression()을 재귀 호출 해서 피연산자 컴파일
    parsePrecedence(PREC_UNARY);
    //연산자 명령어
    switch (operatorType) {
        case TOKEN_BANG:
            emitByte(OP_NOT);
            break;
        case TOKEN_MINUS:
            emitByte(OP_NEGATIVE);
            break;
        default:
            return; //실행되지 않은 코드
    }
}

ParseRule rules[] = {
        [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
        [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
        [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
        [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
        [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
        [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
        [TOKEN_MINUS] = {unary, binary, PREC_TERM},
        [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
        [TOKEN_SEMICOLON] ={NULL, NULL, PREC_NONE},
        [TOKEN_SLASH] ={NULL, binary, PREC_FACTOR},
        [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
//        [TOKEN_QUESTION] = {NULL, ternary, PREC_TERNARY}, // 삼항 연산자
//        [TOKEN_COLON] = {NULL, NULL, PREC_TERNARY}, // 콜론 연산자

        [TOKEN_BANG] ={unary, NULL, PREC_NONE}, // NOT
        [TOKEN_BANG_EQUAL] ={NULL, binary, PREC_EQUALITY},
        [TOKEN_EQUAL] ={NULL, NULL, PREC_NONE},
        [TOKEN_EQUAL_EQUAL] ={NULL, binary, PREC_EQUALITY},
        [TOKEN_GREATER] ={NULL, binary, PREC_COMPARISON},
        [TOKEN_GREATER_EQUAL] ={NULL, binary, PREC_COMPARISON},
        [TOKEN_LESS] ={NULL, binary, PREC_COMPARISON},
        [TOKEN_LESS_EQUAL] ={NULL, binary, PREC_COMPARISON},

        [TOKEN_IDENTIFIER] ={NULL, NULL, PREC_NONE},
        [TOKEN_STRING] ={string, NULL, PREC_NONE},
        [TOKEN_NUMBER] = {number, NULL, PREC_NONE},

        [TOKEN_AND] ={NULL, NULL, PREC_NONE},
        [TOKEN_CLASS] ={NULL, NULL, PREC_NONE},
        [TOKEN_ELSE] ={NULL, NULL, PREC_NONE},
        [TOKEN_FALSE] ={literal, NULL, PREC_NONE},
        [TOKEN_FOR] ={NULL, NULL, PREC_NONE},
        [TOKEN_FUN] ={NULL, NULL, PREC_NONE},
        [TOKEN_IF] ={NULL, NULL, PREC_NONE},
        [TOKEN_NIL] ={literal, NULL, PREC_NONE},
        [TOKEN_OR] ={NULL, NULL, PREC_NONE},
        [TOKEN_PRINT] ={NULL, NULL, PREC_NONE},
        [TOKEN_RETURN] ={NULL, NULL, PREC_NONE},
        [TOKEN_SUPER] ={NULL, NULL, PREC_NONE},
        [TOKEN_THIS] ={NULL, NULL, PREC_NONE},
        [TOKEN_TRUE] ={literal, NULL, PREC_NONE},
        [TOKEN_VAR] ={NULL, NULL, PREC_NONE},
        [TOKEN_WHILE] ={NULL, NULL,PREC_NONE},
        [TOKEN_ERROR] ={NULL, NULL, PREC_NONE},
        [TOKEN_EOF] ={NULL, NULL, PREC_NONE},
};

static void parsePrecedence(Precedence precedence) { //Pratt Parser
    advance();
    ParseFn preFixRule = getRule(parser.previous.type)->prefix;
    if (preFixRule == NULL) { //전위 파서가 없다면 구문 에러가 난 것으로 처리
        error("Expect expression.");
        return;
    }
    preFixRule(); //전위식의 나머지를 컴파일하고 필요로 하는 다른 토큰을 모두 소비한 다음 다시 재귀 호출

    //precedence가 중위 연산자를 허용할 만큼 우선순위가 낮은 경우에만 가능하다
    while (precedence <= getRule(parser.current.type)->precedence) {
        //다음 토큰의 우선순위가 너무 낮거나 중위 연산자가 아니면 작업을 끝낸다
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule();

    }
}

static ParseRule *getRule(TokenType type) {
    return &rules[type];
}

static void expression() { // table driven parser
    parsePrecedence(PREC_ASSIGNMENT);
}


bool compile(const char *source, Chunk *chunk) {
    initScanner(source);
    compilingChunk = chunk; // 바이트 코드를 쓰기 전에 새로운 모듈 변수 초기화

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    expression();
    consume(TOKEN_EOF, "Expect end of expression.");
    endCompiler();

    return !parser.hadError;

}