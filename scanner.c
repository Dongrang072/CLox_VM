#include "scanner.h"

typedef struct {
    const char *start;
    const char *current;
    int line;
    int interpolationDepth;
} Scanner;

Scanner scanner;

static bool isAtEnd() {
    return *scanner.current == '\0';
}

static char advance() {
    scanner.current++;
    return scanner.current[-1];
}

static char peek() {
    return *scanner.current;
}

static char peekNext() {
    if (isAtEnd()) return '\0';
    return scanner.current[1];
}

static bool match(char expected) {
    if (isAtEnd()) return false;
    if (*scanner.current != expected) return false;
    scanner.current++;
    return true;
}

static Token makeToken(TokenType type) { //start와 current 포인터로 토큰의 lexeme 캡쳐
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int) (scanner.current - scanner.start);
    token.line = scanner.line;

    return token;
}

static Token errorToken(const char *message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int) strlen(message);
    token.line = scanner.line;
    return token;
}

static void skipWhiteSpace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if (peekNext() == '/') {
                    //주석은 줄 끝까지 이어진다
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else if (peekNext() == '*') {
                    advance(); // '/'
                    advance(); // '*'
                    while (!isAtEnd() && !(peek() == '*' && peekNext() == '/')) {
                        if (peek() == '\n') scanner.line++;
                        advance();
                    }
                    if (!isAtEnd()) {
                        advance(); // '*'
                        advance(); // '/'
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static TokenType checkKeyword(int start, int length, const char *rest, TokenType type) {
    if (scanner.current - scanner.start == start + length
        && memcmp(scanner.start + start, rest, length) == 0) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

static TokenType identifierType() {
    switch (scanner.start[0]) {
        case 'a':
            return checkKeyword(1, 2, "nd", TOKEN_AND);
        case 'b':
            return checkKeyword(1,4,"reak", TOKEN_BREAK);
        case 'c':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a':
                        return checkKeyword(2, 2,"se", TOKEN_CASE);
                    case 'l':
                        return checkKeyword(2, 3, "ass", TOKEN_CLASS);
                    case 'o':
                        if (scanner.current - scanner.start > 3) {
                            if (scanner.start[2] == 'n') {
                                switch (scanner.start[3]) {
                                    case 's':
                                        return checkKeyword(4, 1, "t", TOKEN_CONST);
                                    case 't':
                                        return checkKeyword(4, 4, "inue", TOKEN_CONTINUE);
                                }
                            }
                        }
                        break;
                }
            }
            break;
        case 'd':
            return checkKeyword(1, 6, "efault", TOKEN_DEFAULT);
        case 'e':
            return checkKeyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a':
                        return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o':
                        return checkKeyword(2, 1, "r", TOKEN_FOR);
                    case 'u' :
                        return checkKeyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i':
            return checkKeyword(1, 1, "f", TOKEN_IF);
        case 'l':
            return checkKeyword(1, 2, "et", TOKEN_LET);
        case 'n':
            return checkKeyword(1, 2, "il", TOKEN_NIL);
        case 'o':
            return checkKeyword(1, 1, "r", TOKEN_OR);
        case 'p':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'r':
                        if (scanner.start[2] == 'i' && scanner.start[3] == 'n') {
                            if (scanner.start[4] == 't') {
                                if (scanner.current - scanner.start > 6 && scanner.start[5] == 'l' && scanner.start[6] == 'n') {
                                    return checkKeyword(1, 6, "rintln", TOKEN_PRINTLN);
                                }
                                return checkKeyword(1, 4, "rint", TOKEN_PRINT);
                            }
                        }
                        break;
                }
            }
            break;
        case 'r':
            return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
        case 's':
            if(scanner.current - scanner.start >1){
                switch (scanner.start[1]) {
                    case 'u':
                        return checkKeyword(2, 3, "per", TOKEN_SUPER);
                    case 'w':
                        return checkKeyword(2,4,"itch", TOKEN_SWITCH);
                }
            }
            break;
        case 't':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'h':
                        return checkKeyword(2, 2, "is", TOKEN_THIS);
                    case 'r':
                        return checkKeyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'w':
            return checkKeyword(1, 4, "hile", TOKEN_WHILE);

    }
    return TOKEN_IDENTIFIER;
}

static Token identifier() {
    while (isAlpha(peek()) || isDigit(peek())) advance();
    return makeToken(identifierType());
}

static Token number() {
    while (isDigit(peek())) advance();
    //소수부 peek()
    if (peek() == '.' && isDigit(peekNext())) {
        //"." consume
        advance();
        while (isDigit(peek())) advance();
    }
    return makeToken(TOKEN_NUMBER);
}

static Token string() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') {
            scanner.line++;
        } else if (peek() == '$' && peekNext() == '{') {
            if (scanner.interpolationDepth >= UINT4_MAX) {
                return errorToken("Interpolation may only nest 15 levels depths.");
            }
            scanner.interpolationDepth++;
            advance(); // '$'를 건너뜀
            Token token =makeToken(TOKEN_INTERPOLATION); // '${' 앞까지의 문자열을 토큰화하고 보간 토큰 생성
            advance(); // '{'를 건너뜀

            return token;
        }
        advance();
    }

    if (isAtEnd()) return errorToken("Unterminated string.");

    advance(); // skip end '"'
    return makeToken(TOKEN_STRING);
}

void initScanner(const char *source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

Token scanToken() {
    skipWhiteSpace();
    scanner.start = scanner.current; //스캔하려는 렉심이 어디에서 시작되었는지를 기억하기 위해 현재 문자를 가르키도록 세팅
    if (isAtEnd()) return makeToken(TOKEN_EOF);

    char c = advance();
    if (isAlpha(c)) return identifier();
    if (isDigit(c)) return number();

    switch (c) {
        case '(':
            return makeToken(TOKEN_LEFT_PAREN);
        case ')':
            return makeToken(TOKEN_RIGHT_PAREN);
        case '{':
            return makeToken(TOKEN_LEFT_BRACE);
        case '}': {
            if (scanner.interpolationDepth > 0) {
                scanner.interpolationDepth--;
                return string();
            }
            return makeToken(TOKEN_RIGHT_BRACE);
        }
        case ';':
            return makeToken(TOKEN_SEMICOLON);
        case ',':
            return makeToken(TOKEN_COMMA);
        case '.':
            return makeToken(TOKEN_DOT);
        case '-':
            return makeToken(TOKEN_MINUS);
        case '+':
            return makeToken(TOKEN_PLUS);
        case '/':
            return makeToken(TOKEN_SLASH);
        case '*':
            return makeToken(TOKEN_STAR);
        case '?':
            return makeToken(TOKEN_QUESTION);
        case ':':
            return makeToken(TOKEN_COLON);
        case '!':
            return makeToken(
                    match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return makeToken(
                    match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>' :
            return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"':
            return string();

    }


    return errorToken("Unexpected character."); //토큰을 스캔하고 반환하지 못했을 경우 함수의 끝에 도달, 인식할 수 없는 문자를 만났다는 뜻
}