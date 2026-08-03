#ifndef MALLOC_H
#define MALLOC_H

#include <stdint.h>
#include <stddef.h>

#define ALIGN16(x) (((x) + 15) & ~15)
#define CHUNK_SIZE 4096   // Pede 1 página por vez ao kernel
#define MIN_SPLIT  32     // Não divide blocos menores que 32 bytes

typedef struct block_header {
    size_t size;                // 8 bytes
    int is_free;                // 4 bytes
    uint32_t padding;           // 4 bytes (Alinhamento para 16 bytes)
    struct block_header* next;  // 8 bytes
    struct block_header* prev;  // 8 bytes
} block_header_t;               // Total: 32 bytes (Perfeito para ALIGN16)

#define HEADER_SIZE sizeof(block_header_t)

void* malloc(size_t size);
void  free(void* ptr);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);

#endif
