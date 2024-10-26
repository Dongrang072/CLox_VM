#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "vm.h"

#include <time.h>

#include "common.h"
#include "debug.h"
#include "compiler.h"
#include "object.h"
#include "memory.h"

VM vm;

static Value clockNative(int argCount, Value *args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void resetStack() {
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.openUpValues = NULL;
}

static void runtimeError(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    // CallFrame *frame = &vm.frames[vm.frameCount - 1]; //vm에서 직접 chunk.ip를 읽는 대신 스택 최상위의 callFrame에서 가져오기
    // size_t instruction = frame->ip - frame->function->chunk.code - 1; //runtimeError()를 호출한 시점의 실패한 명령어는 이전의 명령어다
    // int line = frame->function->chunk.lines[instruction];
    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame *frame = &vm.frames[i];
        ObjFunction *function = frame->closure->function; // -1 because the IP is sitting on the next instruction to be
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, " %s", function->name->chars);
        }
    }

    // fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
}

static void defineNative(const char *name, NativeFn function) {
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1], false);
    pop();
    pop();
}

void initVM() {
    resetStack();
    vm.objects = NULL;
    initTable(&vm.globals); //hash table
    initTable(&vm.strings); //string interning
    defineNative("clock", clockNative);
}

void freeVM() {
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    freeObjects();
}

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(int distance) {
    return vm.stackTop[-1 - distance]; //후에 gc가 트리거되면 피연산자를 스택에 남겨서 관리하기 위함
}

static bool call(ObjClosure *closure, int argCount) {
    if (argCount != closure->function->arity) {
        runtimeError("Expected %d arguments, but got %d", closure->function->arity, argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Stack overflow");
        return false;
    }

    CallFrame *frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static bool callValue(Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_CLOSURE:
                return call(AS_CLOSURE(callee), argCount);
            // case OBJ_FUNCTION:
            //        return call(AS_FUNCTION(callee), argCount);
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                Value result = native(argCount, vm.stackTop - argCount);
                vm.stackTop -= argCount + 1;
                push(result);
                return true;
            }
            default:
                break;
        }
    }
    runtimeError("can only call functions and classes.");
    return false;
}

static ObjUpValue *captureUpValue(Value *local) {
    ObjUpValue *prevUpvalue = NULL;
    ObjUpValue *upvalue = vm.openUpValues;
    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->pNext;
    }
    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }
    ObjUpValue *createdUpValue = newUpValue(local);
    createdUpValue->pNext = upvalue;
    if (prevUpvalue == NULL) {
        vm.openUpValues = createdUpValue;
    } else {
        prevUpvalue->pNext = createdUpValue;
    }

    return createdUpValue;
}

static void closeUpValues(Value *last) {
    while (vm.openUpValues != NULL && vm.openUpValues->location >= last) {
        ObjUpValue *upValue = vm.openUpValues;
        upValue->closed = *upValue->location;
        upValue->location = &upValue->closed;
        vm.openUpValues = upValue->pNext;
    }
}

static void concatenate() {
    ObjString *b = AS_STRING(pop());
    ObjString *a = AS_STRING(pop());

    int length = a->length + b->length;
    char *chars = ALLOCATE(char, length + 1); // + '\0'
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';
    //나중에 복사한 문자열 사본을 메모리에서 해제해야 함
    ObjString *result = takeString(chars, length);
    push(OBJ_VAL(result));
}

static void toString(Value value) {
    switch (value.type) {
        case VAL_BOOL:
            push(OBJ_VAL(copyString(value.as.boolean ? "true" : "false", value.as.boolean ? 4 : 5)));
            break;
        case VAL_NUMBER: {
            char buffer[24];
            int length = sprintf(buffer, "%g", value.as.number);
            push(OBJ_VAL(copyString(buffer, length)));
            break;
        }
        case VAL_OBJ:
            // 객체의 toString 메소드 호출 추후에 추가할 예정
            push(OBJ_VAL(objToString(value.as.obj)));
            break;
        default:
            push(OBJ_VAL(copyString("nil", 3)));
            break;
    }
}

static InterpretResult run() {
    CallFrame *frame = &vm.frames[vm.frameCount - 1];
#define READ_BYTE() (*frame->ip++) //bytecode dispatch
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
    //#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_SHORT() \
    (frame->ip +=2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT_LONG() (frame->closure->function->chunk.constants.values[(READ_BYTE() << 16) | (READ_BYTE() << 8) | READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op) \
    do{\
        if(!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtimeError("Operands must be numbers.");   \
            return INTERPRET_RUNTIME_ERROR;\
        }             \
        double b = AS_NUMBER(pop());                     \
        double a = AS_NUMBER(pop());                     \
        push(valueType(a op b));\
        }while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION //플래그가 켜지면 vm이 실행하기 직전에 디스어셈블한 결과를 매번 동적으로 출력
        printf("          ");
        for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        // disassembleInstruction(vm.chunk, (int) (vm.ip - vm.chunk->code));
        disassembleInstruction(&frame->closure->function->chunk,
                               (int) (frame->ip - frame->closure->function->chunk.code));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_CONSTANT_LONG: {
                Value constant = READ_CONSTANT_LONG();
                push(constant);
                break;
            }
            case OP_NIL:
                push(NIL_VAL);
                break;
            case OP_TRUE:
                push(BOOL_VAL(true));
                break;
            case OP_FALSE:
                push(BOOL_VAL(false));
                break;
            case OP_POP:
                pop();
                break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                // push(vm.stack[slot]);
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                // vm.stack[slot] = peek(0); //할당은 표현식이고 모든 표현식은 값을 만들어낸다. 할당식의 값은 할당된 값 그 자체이므로 pop()은 하지 않는다
                frame->slots[slot] = peek(0);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString *name = READ_STRING(); // 추가된 디버깅 출력
                Value value;
                if (!tableGet(&vm.globals, name, &value)) {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_DEFINE_CONST_GLOBAL: {
                //테이블에 키가 이미 있는지 확인은 하지 않는다.
                //키가 이미 해시 테이블에 있는 경우 그냥 값을 덮어씌운다
                ObjString *name = READ_STRING();
                if (!tableSet(&vm.globals, name, peek(0), true)) {
                    runtimeError("Variable '%s' already defined.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                pop();
                break;
            }
            case OP_DEFINE_LET_GLOBAL: {
                ObjString *name = READ_STRING();
                if (!tableSet(&vm.globals, name, peek(0), false)) {
                    runtimeError("Variable '%s' already defined.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                pop();
                break;
            }
            case OP_SET_GLOBAL: {
                //변수를 set 해도 스택에서 값을 pop()하지 않는다. 할당은 표현식이다
                ObjString *name = READ_STRING();
                bool isConst = findEntry(vm.globals.entries, vm.globals.capacity, name)->isConst;
                if (isConst) {
                    runtimeError("Can't assign to constant variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (tableSet(&vm.globals, name, peek(0), isConst)) {
                    tableDelete(&vm.globals, name);
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(*frame->closure->upValues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upValues[slot]->location = peek(0);
                break;
            }
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_EQUAL_PRESERVE: {
                Value b = pop();
                Value a = peek(0);
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER:
                BINARY_OP(BOOL_VAL, >);
                break;
            case OP_LESS:
                BINARY_OP(BOOL_VAL, <);
                break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                } else {
                    runtimeError("Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT:
                BINARY_OP(NUMBER_VAL, -);
                break;
            case OP_MULTIPLY:
                BINARY_OP(NUMBER_VAL, *);
                break;
            case OP_DIVIDE:
                BINARY_OP(NUMBER_VAL, /);
                break;
            case OP_MODULO: {
                if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                    runtimeError("Operand must be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                push(NUMBER_VAL(fmod(a, b)));
                break;
            }
            case OP_NOT:
                push(BOOL_VAL(isFalsey(pop())));
                break;
            case OP_NEGATIVE:
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            case OP_PRINT:
                printValue(pop());
                break;
            case OP_PRINTLN:
                printValue(pop());
                printf("\n");
                break;
            case OP_TOSTRING: {
                Value value = pop();
                toString(value);
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                // vm.ip += offset;
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(0))) frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int argCount = READ_BYTE();
                if (!callValue(peek(argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_CLOSURE: {
                ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure *closure = newClosure(function);
                push(OBJ_VAL(closure));
                for (int i = 0; i < closure->upValueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint16_t index = READ_BYTE();
                    if (isLocal) {
                        closure->upValues[i] = captureUpValue(frame->slots + index);
                    } else {
                        closure->upValues[i] = frame->closure->upValues[index];
                    }
                }

                break;
            }
            case OP_CLOSE_UPVALUE:
                closeUpValues(vm.stackTop - 1);
                pop();
                break;
            case OP_RETURN: {
                Value result = pop();
                closeUpValues(frame->slots);
                vm.frameCount--;
                if (vm.frameCount == 0) {
                    pop();
                    return INTERPRET_OK;
                }
                vm.stackTop = frame->slots;
                push(result);
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
        }
    }
#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_CONSTANT_LONG
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char *source) {
    // Chunk chunk;
    // initChunk(&chunk);
    // if (!compile(source, &chunk)) {
    //     freeChunk(&chunk);
    //     return INTERPRET_COMPILE_ERROR;
    // }
    // vm.chunk = &chunk;
    // vm.ip = vm.chunk->code;
    ObjFunction *function = compile(source);
    if (function == NULL) return INTERPRET_RUNTIME_ERROR;
    push(OBJ_VAL(function));
    ObjClosure *closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    callValue(OBJ_VAL(closure), 0);

    return run();
}
