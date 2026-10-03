#ifndef ZS_COMMON_H
#define ZS_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Enteros con tamaño estricto
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef intptr_t isize;

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef uintptr_t usize;

typedef float  f32;
typedef double f64;

typedef uint32_t char32;

#define ZS_ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

#define ZS_FATAL(fmt, ...) do { \
    fprintf(stderr, "[FATAL %s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    exit(1); \
} while(0)

#endif // ZS_COMMON_H
