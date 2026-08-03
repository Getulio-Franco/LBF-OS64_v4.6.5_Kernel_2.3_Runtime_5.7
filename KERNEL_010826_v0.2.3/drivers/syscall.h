/**
 * ============================================================================
 * SYSCALL INTERFACE - V4.0 (ASYNC TASK_D EDITION)
 * ============================================================================
 */

#ifndef DRIVERS_SYSCALL_H
#define DRIVERS_SYSCALL_H

#include <stdint.h>
#include <stddef.h>

/* --- I/O E VFS --- */
#define SYS_OPEN            0
#define SYS_READ            1
#define SYS_WRITE           2
#define SYS_CLOSE           3
#define SYS_CLEAR           4
#define SYS_MEM_INFO        5
#define SYS_SERIAL_WRITE    6
#define SYS_SERIAL_READ     7
#define SYS_SERIAL_OPEN     8
#define SYS_SERIAL_CLOSE    9

/* --- SISTEMA DE ARQUIVOS --- */
#define SYS_FATLS           10
#define SYS_FATCAT          11
#define SYS_FATRM           12  /* Remove arquivos/diretórios */
#define SYS_FATCP           13  /* Copia arquivos */
#define SYS_FATAPPEND       15 
#define SYS_FATRENAME       14 //não foi testado ainda so incrementado.
#define SYS_MKDIR           16
#define SYS_CHDIR           17
#define SYS_XCOPY           18

/* --- PROCESSOS --- */
#define SYS_EXEC            19
#define SYS_EXIT            20
#define SYS_KILL            21
#define SYS_PS              22
#define SYS_GETPID          23
#define SYS_FATSTAT         24 //não foi testado ainda so incrementado.
#define SYS_GET_PS_DATA     25
#define SYS_FATREADDIR      27
#define SYS_SET_DRIVE       28

/* --- MEMÓRIA E TEMPO --- */
#define SYS_SLEEP           42
#define SYS_GET_TICKS       43
#define SYS_SBRK            45
#define SYS_GET_PARAM       46

/* --- VÍDEO E INPUT BRUTO (A Nova Base) --- */
#define SYS_GET_MOUSE       60  /* Lê posição e botões do mouse */
#define SYS_GET_KEY         61  /* Lê tecla pressionada */
#define SYS_VIDEO_FLIP      62  /* Copia o buffer do Ring 3 para a VRAM */
#define SYS_GET_LFB_CRITICAL_DATA 63

#define SYS_TESTUSB         64
#define SYS_EHCI            65
#define SYS_TESTSTORAGE     66
#define SYS_GET_PCI_DEVICE  67
#define SYS_LSPCI_TERMINAL  68

/* ============================================================================
 * DEFINIÇÃO DA SYSCALL DE DEBUG / LOGS DO KERNEL
 * ============================================================================ */
#define SYS_READ_KERNEL_LOG  69

/**
 * @brief Inicializa MSRs para habilitar instruções SYSCALL/SYSRET.
 */
void init_syscall_msrs(void);

/**
 * @brief Handler central (Dispatcher).
 * * Mapeamento de Registradores conforme System V ABI estendida:
 * RAX: ID da Syscall
 * RDI: arg1
 * RSI: arg2
 * RDX: arg3
 * R10: arg4
 * R8:  arg5
 * * @return Resultado da operação (RAX) para o Ring 3.
 */
uint64_t syscall_handler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

uint64_t sys_get_kernel_data(uint64_t param_id);

#endif /* DRIVERS_SYSCALL_H */
