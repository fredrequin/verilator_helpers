#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include "musashi/m68k.h"

#define FLG_C(r) (r & 1)  ? '1' : '0'
#define FLG_V(r) (r & 2)  ? '1' : '0'
#define FLG_Z(r) (r & 4)  ? '1' : '0'
#define FLG_N(r) (r & 8)  ? '1' : '0'
#define FLG_X(r) (r & 16) ? '1' : '0'

void         m68k_instr_hook(void) { }
unsigned int m68k_read_disassembler_8  (unsigned int addr) { return m68k_read_memory_8(addr); }
unsigned int m68k_read_disassembler_16 (unsigned int addr) { return m68k_read_memory_16(addr); }
unsigned int m68k_read_disassembler_32 (unsigned int addr) { return m68k_read_memory_32(addr); }

// Hack to integrate musashi into the C++ framework that verilator needs
#include "musashi/m68kopac.c"
#include "musashi/m68kopdm.c"
#include "musashi/m68kopnz.c"
#include "musashi/m68kops.c"
#include "musashi/m68kcpu.c"
#include "musashi/m68kdasm.c"

#ifdef __cplusplus
}
#endif
