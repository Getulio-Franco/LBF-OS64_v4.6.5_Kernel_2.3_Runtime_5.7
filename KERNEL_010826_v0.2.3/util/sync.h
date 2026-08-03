typedef volatile int spinlock_t;
#define SPINLOCK_UNLOCKED 0

static inline void spin_lock(spinlock_t* lock) {
    // Tenta setar o lock para 1. Se já for 1, ele está ocupado, então continua no loop.
    while (__sync_lock_test_and_set(lock, 1)) {
        // Pequena otimização de hardware (instrução pause) para não fritar a CPU 
        // ou sobrecarregar o barramento de memória enquanto espera
        while (*lock) {
            __asm__ volatile ("pause"); 
        }
    }
}

static inline void spin_unlock(spinlock_t* lock) {
    // Libera a trava voltando para 0 de forma atômica
    __sync_lock_release(lock);
}
