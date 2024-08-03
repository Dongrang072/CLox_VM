#ifndef CLOX_COMMON_H
#define CLOX_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <malloc.h>
#include <limits.h>
#include <string.h>

#define DEBUG_PRINT_CODE //flag가 켜지면 기존 debug모듈을 사용하여 chunk의 바이트 코드 출력
#define DEBUG_TRACE_EXECUTION

#define UINT8_COUNT (UINT8_MAX + 1)
#define UINT4_MAX 15
#define MAX_CASES 256
#define MAX_CONST_INDEX 16777216

#endif //CLOX_COMMON_H
