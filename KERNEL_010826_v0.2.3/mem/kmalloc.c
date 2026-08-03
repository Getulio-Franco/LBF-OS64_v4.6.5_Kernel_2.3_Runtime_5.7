#include "mem/heap.h"
#include "vmm.h"
#include "pmm.h"
#include "util/string.h"

typedef struct cabecalho_bloco {
    size_t tamanho;
    int esta_livre;
    uint32_t padding;
    struct cabecalho_bloco* proximo;
    struct cabecalho_bloco* anterior;
} cabecalho_bloco_t;

static uint64_t heap_atual = HEAP_START_SAFE;
static uint64_t heap_mapeado_fim = HEAP_START_SAFE;
static cabecalho_bloco_t* lista_livre_inicio = NULL;
static cabecalho_bloco_t* lista_livre_fim = NULL;
static volatile int trava_heap = 0;
static uint64_t rflags_salvos = 0;

/* Trava o Kernel e desliga interrupções para segurança total */
static void trava_adquirir() { 
    // Lê o RFLAGS, salva, e desliga interrupções
    __asm__ volatile(
        "pushfq\n\t"
        "popq %0\n\t"
        "cli"
        : "=r"(rflags_salvos)
        :
        : "memory"
    );
    while (__sync_lock_test_and_set(&trava_heap, 1)); 
}

/* Libera a trava e religa as interrupções */
static void trava_liberar() { 
    __sync_lock_release(&trava_heap); 
    // Só religa se estava ligado antes (Bit 9 do RFLAGS é o IF - Interrupt Flag)
    if (rflags_salvos & (1 << 9)) {
        __asm__ volatile("sti");
    }
}

/* Expande a memória e avisa a CPU (CRÍTICO para o Memo) */
static int expandir_heap(size_t necessario) {
    // PROTEÇÃO: Impede que o Heap cresça além do LIMITE_HEAP (64MB)
    if (heap_atual + necessario >= ENDERECO_MAX) {
        return 0; // Out of Memory (OOM) - Heap estourou o limite!
    }

    while (heap_atual + necessario >= heap_mapeado_fim) {
        void* fisico = pmm_alloc_block();
        if (!fisico) return 0;

        vmm_map_page_4kb(heap_mapeado_fim, (uint64_t)fisico, PAGE_PRESENT | PAGE_WRITE);
        
        asm volatile("invlpg (%0)" :: "r" (heap_mapeado_fim) : "memory");
        heap_mapeado_fim += 4096;
    }
    return 1;
}

void heap_init() {
    trava_adquirir();
    if (expandir_heap(4096)) {
        lista_livre_inicio = (cabecalho_bloco_t*)HEAP_START_SAFE;
        lista_livre_inicio->tamanho = 4096 - sizeof(cabecalho_bloco_t);
        lista_livre_inicio->esta_livre = 1;
        lista_livre_inicio->proximo = NULL;
        lista_livre_inicio->anterior = NULL;
        lista_livre_fim = lista_livre_inicio;
        heap_atual = HEAP_START_SAFE + 4096;
    }
    trava_liberar();
}

void* kmalloc(size_t tamanho) {
    if (tamanho == 0) return NULL;
    
    // Alinhamento de 16 bytes
    tamanho = (tamanho + 15) & ~15; 

    trava_adquirir();
    cabecalho_bloco_t* atual = lista_livre_inicio;

    while (atual != NULL) {
        if (atual->esta_livre && atual->tamanho >= tamanho) {
            if (atual->tamanho >= tamanho + sizeof(cabecalho_bloco_t) + 16) {
                cabecalho_bloco_t* novo_bloco = (cabecalho_bloco_t*)((uint8_t*)atual + sizeof(cabecalho_bloco_t) + tamanho);
                novo_bloco->tamanho = atual->tamanho - tamanho - sizeof(cabecalho_bloco_t);
                novo_bloco->esta_livre = 1;
                novo_bloco->proximo = atual->proximo;
                novo_bloco->anterior = atual;
                
                if (novo_bloco->proximo) novo_bloco->proximo->anterior = novo_bloco;
                else lista_livre_fim = novo_bloco;
                
                atual->tamanho = tamanho;
                atual->proximo = novo_bloco;
            }
            atual->esta_livre = 0;
            trava_liberar();
            return (void*)((uint8_t*)atual + sizeof(cabecalho_bloco_t));
        }
        atual = atual->proximo;
    }

    size_t tamanho_total = sizeof(cabecalho_bloco_t) + tamanho;
    if (!expandir_heap(tamanho_total)) {
        trava_liberar();
        return NULL; 
    }

    cabecalho_bloco_t* novo = (cabecalho_bloco_t*)heap_atual;
    novo->tamanho = tamanho;
    novo->esta_livre = 0;
    novo->proximo = NULL;
    novo->anterior = lista_livre_fim;

    if (lista_livre_fim != NULL) lista_livre_fim->proximo = novo;
    else lista_livre_inicio = novo;
    
    lista_livre_fim = novo;
    heap_atual += tamanho_total;

    trava_liberar();
    return (void*)((uint8_t*)novo + sizeof(cabecalho_bloco_t));
}

void kfree(void* ponteiro) {
    if (!ponteiro) return;
    trava_adquirir();

    cabecalho_bloco_t* bloco = (cabecalho_bloco_t*)((uint8_t*)ponteiro - sizeof(cabecalho_bloco_t));
    bloco->esta_livre = 1;

    // Aglutinar blocos para evitar fragmentação
    if (bloco->proximo && bloco->proximo->esta_livre) {
        uint8_t* fim_deste = (uint8_t*)bloco + sizeof(cabecalho_bloco_t) + bloco->tamanho;
        if (fim_deste == (uint8_t*)bloco->proximo) {
            bloco->tamanho += sizeof(cabecalho_bloco_t) + bloco->proximo->tamanho;
            bloco->proximo = bloco->proximo->proximo;
            if (bloco->proximo) bloco->proximo->anterior = bloco;
            else lista_livre_fim = bloco;
        }
    }

    if (bloco->anterior && bloco->anterior->esta_livre) {
        cabecalho_bloco_t* ant = bloco->anterior;
        uint8_t* fim_ant = (uint8_t*)ant + sizeof(cabecalho_bloco_t) + ant->tamanho;
        if (fim_ant == (uint8_t*)bloco) {
            ant->tamanho += sizeof(cabecalho_bloco_t) + bloco->tamanho;
            ant->proximo = bloco->proximo;
            if (bloco->proximo) bloco->proximo->anterior = ant;
            else lista_livre_fim = ant;
        }
    }
    trava_liberar();
}

void kmalloc_stats(size_t* usado, size_t* livre) {
    size_t acc_usado = 0, acc_livre = 0;
    trava_adquirir();
    cabecalho_bloco_t* atual = lista_livre_inicio;
    while (atual != NULL) {
        if (atual->esta_livre) acc_livre += atual->tamanho;
        else acc_usado += (sizeof(cabecalho_bloco_t) + atual->tamanho);
        atual = atual->proximo;
    }
    if (heap_mapeado_fim > heap_atual) acc_livre += (heap_mapeado_fim - heap_atual);
    *usado = acc_usado;
    *livre = acc_livre;
    trava_liberar();
}

void get_system_memory_info(size_t* used, size_t* free) {
    if (used && free) {
        kmalloc_stats(used, free);
    }
}
