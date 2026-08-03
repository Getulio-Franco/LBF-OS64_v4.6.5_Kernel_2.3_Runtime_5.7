/**
 * ============================================================================
 * LBF OS KERNEL (RING0) - STRING & MEMORY UTILS (V1.2) data 03/07/26
 * ============================================================================
 */

#include "string.h"

// =========================================================================
// 🧠 Implementações de Gerenciamento de Memória (Otimizadas para 64 bits no Kernel)
// =========================================================================

/*void* memset(void* s, int c, size_t n) {
    // 🔥 VERSÃO CORRIGIDA PARA GCC -m64
    __asm__ __volatile__ (
        "movq %0, %%rdi\n\t"     // Move 's' (64-bits) para RDI
        "movl %1, %%eax\n\t"     // 🛠️ CORREÇÃO: Move 'c' (32-bits) para EAX (zera a parte alta de RAX)
        "movq %2, %%rdx\n\t"     // Move 'n' (64-bits) para RDX

        // 1. Replicar o byte por todos os 8 bytes de RAX
        "andq $0xFF, %%rax\n\t"  // Isola o byte mais baixo
        "movq %%rax, %%r8\n\t"   
        "shlq $8, %%rax\n\t"     
        "orq  %%r8, %%rax\n\t"   
        "movq %%rax, %%r8\n\t"   
        "shlq $16, %%rax\n\t"    
        "orq  %%r8, %%rax\n\t"   
        "movq %%rax, %%r8\n\t"   
        "shlq $32, %%rax\n\t"    
        "orq  %%r8, %%rax\n\t"   

        // 2. Preencher em blocos de 64 bits (QWORDS)
        "movq %%rdx, %%rcx\n\t"  
        "shrq $3, %%rcx\n\t"     
        "cld\n\t"                
        "rep stosq\n\t"          

        // 3. Limpar o resto (n % 8)
        "movq %%rdx, %%rcx\n\t"  
        "andq $7, %%rcx\n\t"     
        "rep stosb"              
        :
        : "r"(s), "r"(c), "r"(n)
        : "rdi", "rax", "rdx", "rcx", "r8", "memory"
    );
    return s;
}*/

/*void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}*/

/*void* memcpy(void* dest, const void* src, size_t n) {
    // 🔥 ULTRA-OTIMIZAÇÃO NO RING0: Conversa direta com o hardware/barramento em 64 bits.
    // Sem condicionais complexas no Kernel para atrasar drivers de disco (AHCI/IDE).
    __asm__ __volatile__ (
        "movq %0, %%rdi\n\t"     // Destino -> RDI
        "movq %1, %%rsi\n\t"     // Origem  -> RSI
        "movq %2, %%rdx\n\t"     // Bytes   -> RDX
        "movq %%rdx, %%rcx\n\t"  
        "shrq $3, %%rcx\n\t"     // n / 8 (Blocos de 64 bits)
        "cld\n\t"                
        "rep movsq\n\t"          // Cópia em rajada massiva de QWORDS
        "movq %%rdx, %%rcx\n\t"  
        "andq $7, %%rcx\n\t"     // Resto da divisão (n % 8)
        "rep movsb"              // Copia o que sobrou byte a byte
        :
        : "r"(dest), "r"(src), "r"(n)
        : "rdi", "rsi", "rdx", "rcx", "memory"
    );
    return dest;
}*/

/*void* memcpy(void* dest, const void* src, size_t n) {
    void* d = dest;
    const void* s = src;
    size_t qwords = n / 8;
    size_t bytes = n % 8;

    __asm__ __volatile__ (
        "cld\n\t"
        "rep movsq\n\t"
        "movq %4, %%rcx\n\t"
        "rep movsb"
        : "+D"(d), "+S"(s), "+c"(qwords) // Usa RDI, RSI, RCX diretamente como saída/entrada
        : "r"(dest), "r"(bytes)
        : "memory", "cc"                 // Preserva a coerência de memória e flags
    );

    return dest;
}*/

void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) return (int)p1[i] - (int)p2[i];
    }
    return 0;
}

// =========================================================================
// 🔤 Funções de String Estáveis e Seguras
// =========================================================================

size_t strlen(const char *str) {
    size_t len = 0;
    while (str && str[len]) len++;
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for ( ; i < n; i++)
        dest[i] = '\0';
    return dest;
}

char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

char* strncat(char* dest, const char* src, size_t n) {
    size_t dest_len = strlen(dest);
    size_t i;
    for (i = 0 ; i < n && src[i] != '\0' ; i++)
        dest[dest_len + i] = src[i];
    dest[dest_len + i] = '\0';
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    if (n == 0) return 0;
    while (n > 1 && *s1 && (*s1 == *s2)) {
        s1++; s2++; n--;
    }
    return (*(unsigned char *)s1 - *(unsigned char *)s2);
}

char* strchr(const char* s, int c) {
    while (*s != (char)c) {
        if (!*s++) return 0;
    }
    return (char*)s;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    
    while (*haystack) {
        const char* h = haystack;
        const char* n = needle;
        
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n) return (char*)haystack;
        haystack++;
    }
    return 0;
}

// =========================================================================
// ⚙️ Conversões de Dados e Formatação Numérica
// =========================================================================

int atoi(const char* str) {
    int res = 0;
    int sign = 1;
    if (!str) return 0;

    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') str++;

    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return sign * res;
}

void itoa(uint64_t n, char* str, int base) {
    int i = 0;
    if (n == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    char temp[65]; 
    int j = 0;
    while (n > 0) {
        uint64_t rem = n % base;
        temp[j++] = (rem < 10) ? (rem + '0') : (rem - 10 + 'A');
        n /= base;
    }

    while (j > 0) str[i++] = temp[--j];
    str[i] = '\0';
}

void int_to_string(int n, char* buffer) {
    if (n == 0) {
        strcpy(buffer, "0");
        return;
    }
    if (n < 0) {
        *buffer++ = '-';
        n = -n;
    }
    itoa((uint64_t)n, buffer, 10);
}

void hex_to_string(uintptr_t n, char* buffer) {
    buffer[0] = '0';
    buffer[1] = 'x';
    itoa((uint64_t)n, buffer + 2, 16);
}

// =========================================================================
// ✂️ Tokenizador de Strings
// =========================================================================

static int is_delimiter(char c, const char* delimiters) {
    while (*delimiters) {
        if (c == *delimiters) return 1;
        delimiters++;
    }
    return 0;
}

char* strtok(char* str, const char* delimiters) {
    static char* token_last_position = 0;

    if (str != 0) {
        token_last_position = str;
    }

    if (token_last_position == 0 || *token_last_position == '\0') {
        return 0;
    }

    while (*token_last_position && is_delimiter(*token_last_position, delimiters)) {
        token_last_position++;
    }

    if (*token_last_position == '\0') {
        token_last_position = 0;
        return 0;
    }

    char* token_start = token_last_position;

    while (*token_last_position && !is_delimiter(*token_last_position, delimiters)) {
        token_last_position++;
    }

    if (*token_last_position != '\0') {
        *token_last_position = '\0';
        token_last_position++;
    } else {
        token_last_position = 0;
    }

    return token_start;
}
