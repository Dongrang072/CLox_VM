#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    PREC_TERNARY, // a ? b : c ternary()
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

typedef void(*ParseFn)(bool canAssgin);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name; //변수 이름
    int depth;
    bool isConst; //const  추가
} Local;

typedef struct {
    Local locals[UINT8_COUNT];
    int localCount; //스코프에 있는 지역 변수의 개수(사용중인 배열 슬롯의 개수) 추적
    int scopeDepth; //'컴파일중인' 현재 코드의 비트를 둘러싼 블록의 개수
} Compiler;

Parser parser;
Compiler *current = NULL; //원칙적으로라면 Compiler 포인터를 받는 매개변수를 프론트엔드에 있는 각 매게 변수에 전달하겠지만....
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

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
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

static void initCompiler(Compiler *compiler) {
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    current = compiler;
}

static void endCompiler() {
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

static void beginScope() {
    current->scopeDepth++;
}

static void endScope() {
    current->scopeDepth--;

    while (current->localCount > 0 &&
           current->locals[current->localCount - 1].depth > current->scopeDepth) {
        emitByte(OP_POP); //지역변수가 스코프를 벗어나면 해당 슬롯은 더 이상 필요가 없다
        current->localCount--;
    }
}

static void expression();

static void statement();

static void declaration();

static ParseRule *getRule(TokenType type);

static void parsePrecedence(Precedence precedence);

static uint8_t identifierConstant(Token *name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token *a, Token *b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, Token *name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local *local = &(compiler->locals[i]);
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) { //스코프의 깊이가 -1이면 변수 초기자 안에서 해당 변수를 참조한 것
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1; //전역변수임을 나타냄
}


static void addLocal(Token name, bool isConst) {
    if (current->localCount == UINT8_COUNT) { //VM은 스코프에 최대 256개의 지역 변수까지만 지원함
        error("Too many local variables in function.");
        return;
    }
    //인덱스는 1바이트 크기의 피연산자에 저장됨
    Local *local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1; //초기화되지 않은 상태 표시, 나중에 변수의 초기자 컴파일이 끝나면 markInitialized()로 초기화가 된 것으로 표시, 선언만 된 상태
    local->isConst = isConst;
}

static void declareVariable(bool isConst) {
    if (current->scopeDepth == 0) return;

    Token *name = &parser.previous;

    for (int i = current->localCount - 1; i >= 0; i--) {
        Local *local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }
        if (identifiersEqual(name, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }
    addLocal(*name, isConst);
}

static uint8_t parseVariable(const char *errorMessage, bool isConst) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable(isConst);
    if (current->scopeDepth > 0) return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialized() {
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global) {
    //아직 선언만 된 상태
    if (current->scopeDepth > 0) {
        markInitialized(); //초기화됨을 확인
        return;
    }
    emitBytes(OP_DEFINE_GLOBAL, global);
}

static void binary(bool canAssgin) {
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

static void literal(bool canAssgin) {
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

static void grouping(bool canAssgin) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

static void number(bool canAssgin) {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void string(bool canAssgin) {
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2))); // 전후 " 제거
}

static void namedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1) {
        if (current->locals[arg].isConst && canAssign && match(TOKEN_EQUAL)) {
            error("Can not assign to const variable.");
        }
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else {
        arg = identifierConstant(&name);
        if(canAssign && match(TOKEN_EQUAL)){ //전역 변수의 재할당 검증
            expression();
            emitBytes(OP_SET_GLOBAL, (uint8_t) arg);
            return;
        }
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }
    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(setOp, (uint8_t) arg);
    } else {
        emitBytes(getOp, (uint8_t) arg);
    }
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
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

static void unary(bool canAssgin) {
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

        [TOKEN_IDENTIFIER] ={variable, NULL, PREC_NONE},
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
        [TOKEN_CONST] = {NULL, NULL, PREC_NONE},
        [TOKEN_LET] ={NULL, NULL, PREC_NONE},
        [TOKEN_WHILE] ={NULL, NULL, PREC_NONE},
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
    bool canAssgin = precedence <= PREC_ASSIGNMENT; //할당은 우선순위가 가장 낮은 표현식임을 명심하기
    preFixRule(canAssgin); //전위식의 나머지를 컴파일하고 필요로 하는 다른 토큰을 모두 소비한 다음 다시 재귀 호출

    //precedence가 중위 연산자를 허용할 만큼 우선순위가 낮은 경우에만 가능하다
    while (precedence <= getRule(parser.current.type)->precedence) {
        //다음 토큰의 우선순위가 너무 낮거나 중위 연산자가 아니면 작업을 끝낸다
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssgin);

    }
    if (canAssgin && match(TOKEN_EQUAL))
        error("Invalid assignment target.");
}

static ParseRule *getRule(TokenType type) {
    return &rules[type];
}

static void expression() { // table driven parser
    parsePrecedence(PREC_ASSIGNMENT);
}

static void block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) { //사용자가 }를 까먹을 수도 있으므로 eof 체크
        declaration();
    }
     consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void varDeclaration(bool isConst) {
    //변수 이름에 대한 식별자 토큰 소비, 렉심을 청크의 상수 테이블에 문자열로 추가, 해당 상수 테이블의 인덱스를 return
    uint8_t global = parseVariable("Expect variable name.", isConst);

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ':' after variable declaration.");

    defineVariable(global);
}

static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP); //시맨틱상 표현문은 표현식을 평가는 하지만 그 결과는 버린다
}

static void printStatement() {
    expression(); //print문은 표현식을 평가하고 그 결과를 출력함
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON)
            return;
        switch (parser.current.type) { //문장 경계는 세미콜론이나 뒤에 문장으로 시작하는 토큰이 있는지 여부로 알 수 있음
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_LET:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:
                //none
        }
        advance();
    }
}

static void declaration() {
    if(match(TOKEN_CONST)){
        varDeclaration(true);
    }else if (match(TOKEN_LET)) {
        varDeclaration(false);
    } else {
        statement();
    }
    if (parser.panicMode) synchronize(); //panicModeError 복구 후 컴파일 에러 갯수를 최소화 하기
}

static void statement() {
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else { //print 키워드가 안 보이면 표현문이다
        expressionStatement();
    }
}

bool compile(const char *source, Chunk *chunk) {
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler);
    compilingChunk = chunk; // 바이트 코드를 쓰기 전에 새로운 모듈 변수 초기화

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    while (!match(TOKEN_EOF)) { //eof를 못 찾아서 무한루프
        declaration();
    }
    endCompiler();

    return !parser.hadError;

}