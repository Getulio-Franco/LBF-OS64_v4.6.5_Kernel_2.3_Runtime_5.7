/**
 * ============================================================================
 * STRING & MEMORY UTILS - CABEÇALHO (V1.2) data 22/06/26
 * ============================================================================
 */

#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stddef.h>

// --- Operações de Memória ---
void* memset(void* s, int c, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

// --- Operações de String ---
size_t strlen(const char *str);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char* strchr(const char* s, int c);
char* strstr(const char* haystack, const char* needle); // Solução do Warning!
char* strtok(char* str, const char* delimiters);

// --- Conversões e Formatação ---
int atoi(const char* str);
void itoa(uint64_t n, char* str, int base);
void int_to_string(int n, char* buffer);
void hex_to_string(uintptr_t n, char* buffer);

#endif // STRING_H
