// Copyright 2014-2022 Frederic Requin
//
// License: BSD
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions 
// are met:
//   - Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
//   - Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer 
//     in the documentation and/or other materials provided with the 
//     distribution.
//   - Neither the name of the author nor the names of its contributors 
//     may be used to endorse or promote products derived from this 
//     software without specific prior written permission.
//
// Mico32 trace:
// -------------
//  - It is designed to work with "Verilator" (www.veripool.org)
//  - Based on the "LatticeMico32 Processor Reference Manual" from Lattice
//  - It emulates and traces the LM32 instructions
//  - It detects mismatches between trace and simulation
//  - It is intended to be connected to an LM32 verilog core
//  - It supports segmented traces
//  - Memory footprint is minimal
//
// TODO:
//  - Add support to custom instructions

#include "verilated.h"
#include "lm32_trace.h"
#include <stdlib.h>
#include <stdio.h>

enum
{
    OP_SRUI = 0, // 0x00, I-Type : srui    rY,rX,#uimm5
    OP_NORI,     // 0x01, I-Type : nori    rY,rX,#uimm16
    OP_MULI,     // 0x02, I-Type : muli    rY,rX,#simm16
    OP_SH,       // 0x03, I-Type : sh      simm16(rX),rY
    OP_LB,       // 0x04, I-Type : lb      rY,simm16(rX)
    OP_SRI,      // 0x05, I-Type : sri     rY,rX,#uimm5
    OP_XORI,     // 0x06, I-Type : xori    rY,rX,#uimm16
    OP_LH,       // 0x07, I-Type : lh      rY,simm16(rX)
    OP_ANDI,     // 0x08, I-Type : andi    rY,rX,#uimm16
    OP_XNORI,    // 0x09, I-Type : xnori   rY,rX,#uimm16
    OP_LW,       // 0x0A, I-Type : lw      rY,simm16(rX)
    OP_LHU,      // 0x0B, I-Type : lhu     rY,simm16(rX)
    OP_SB,       // 0x0C, I-Type : sb      simm16(rX),rY
    OP_ADDI,     // 0x0D, I-Type : addi    rY,rX,#simm16
    OP_ORI,      // 0x0E, I-Type : ori     rY,rX,#uimm16
    OP_SLI,      // 0x0F, I-Type : sli     rY,rX,#uimm5
    OP_LBU,      // 0x10, I-Type : lbu     rY,simm16(rX)
    OP_BE,       // 0x11, I-Type : be      rX,rY,#simm16
    OP_BG,       // 0x12, I-Type : bg      rX,rY,#simm16
    OP_BGE,      // 0x13, I-Type : bge     rX,rY,#simm16
    OP_BGEU,     // 0x14, I-Type : bgeu    rX,rY,#simm16
    OP_BGU,      // 0x15, I-Type : bgu     rX,rY,#simm16
    OP_SW,       // 0x16, I-Type : sw      simm16(rX),rY
    OP_BNE,      // 0x17, I-Type : bne     rX,rY,#simm16
    OP_ANDHI,    // 0x18, I-Type : andhi   rY,rX,#uimm16
    OP_CMPEI,    // 0x19, I-Type : cmpei   rY,rX,#simm16
    OP_CMPGI,    // 0x1A, I-Type : cmpgi   rY,rX,#simm16
    OP_CMPGEI,   // 0x1B, I-Type : cmpgei  rY,rX,#simm16
    OP_CMPGEUI,  // 0x1C, I-Type : cmpgeui rY,rX,#uimm16
    OP_CMPGUI,   // 0x1D, I-Type : cmpgui  rY,rX,#uimm16
    OP_ORHI,     // 0x1E, I-Type : orhi    rY,rX,#uimm16
    OP_CMPNEI,   // 0x1F, I-Type : cmpnei  rY,rX,#simm16
    OP_SRU,      // 0x20, R-Type : sru     rZ,rX,rY
    OP_NOR,      // 0x21, R-Type : nor     rZ,rX,rY
    OP_MUL,      // 0x22, R-Type : mul     rZ,rX,rY
    OP_DIVU,     // 0x23, R-Type : divu    rZ,rX,rY
    OP_RCSR,     // 0x24, R-Type : rcsr    rZ,csr
    OP_SR,       // 0x25, R-Type : sr      rZ,rX,rY
    OP_XOR,      // 0x26, R-Type : xor     rZ,rX,rY
    OP_DIV,      // 0x27, R-Type : div     rZ,rX,rY
    OP_AND,      // 0x28, R-Type : and     rZ,rX,rY
    OP_XNOR,     // 0x29, R-Type : xnor    rZ,rX,rY
    OP_2A,       // 0x2A
    OP_RAISE,    // 0x2B, R-Type : raise   rZ,uimm5
    OP_SEXTB,    // 0x2C, R-Type : sextb   rZ,rX
    OP_ADD,      // 0x2D, R-Type : add     rZ,rX,rY
    OP_OR,       // 0x2E, R-Type : or      rZ,rX,rY
    OP_SL,       // 0x2F, R-Type : sl      rZ,rX,rY
    OP_B,        // 0x30, R-Type : b       rX
    OP_MODU,     // 0x31, R-Type : modu    rZ,rX,rY
    OP_SUB,      // 0x32, R-Type : sub     rZ,rX,rY
    OP_USER,     // 0x33, C-Type : user    #uimm11,rZ,rX,rY
    OP_WCSR,     // 0x34, R-Type : wcsr    csr,rY
    OP_MOD,      // 0x35, R-Type : mod     rZ,rX,rY
    OP_CALL,     // 0x36, R-Type : call    rX
    OP_SEXTH,    // 0x37, R-Type : sexth   rZ,rX
    OP_BI,       // 0x38, J-Type : bi      simm26
    OP_CMPE,     // 0x39, R-Type : cmpe    rZ,rX,rY
    OP_CMPG,     // 0x3A, R-Type : cmpg    rZ,rX,rY
    OP_CMPGE,    // 0x3B, R-Type : cmpge   rZ,rX,rY
    OP_CMPGEU,   // 0x3C, R-Type : cmpgeu  rZ,rX,rY
    OP_CMPGU,    // 0x3D, R-Type : cmpgu   rZ,rX,rY
    OP_CALLI,    // 0x3E, J-Type : calli   simm26
    OP_CMPNE,    // 0x3F, R-Type : cmpne   rZ,rX,rY
    OP_TOTAL
};

// Hexadecimal conversion table
static const char hex_dig[16] =
{
  '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
};

// Mnemonics table
static const char opc_str[OP_TOTAL][8] =
{
    "srui   ", "nori   ", "muli   ", "sh     ",
    "lb     ", "sri    ", "xori   ", "lh     ",
    "andi   ", "xnori  ", "lw     ", "lhu    ",
    "sb     ", "addi   ", "ori    ", "sli    ",
    "lbu    ", "be     ", "bg     ", "bge    ",
    "bgeu   ", "bgu    ", "sw     ", "bne    ",
    "andhi  ", "cmpei  ", "cmpgi  ", "cmpgei ",
    "cmpgeui", "cmpgui ", "orhi   ", "cmpnei ",
    "sru    ", "nor    ", "mul    ", "divu   ",
    "rcsr   ", "sr     ", "xor    ", "div    ",
    "and    ", "xnor   ", "$2A ?? ", "raise  ",
    "sextb  ", "add    ", "or     ", "sl     ",
    "b      ", "modu   ", "sub    ", "user   ",
    "wcsr   ", "mod    ", "call   ", "sexth  ",
    "bi     ", "cmpe   ", "cmpg   ", "cmpge  ",
    "cmpgeu ", "cmpgu  ", "calli  ", "cmpne  "
};

// Registers names
static const char reg_str[32][4] =
{
    "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
    "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",
    "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
    "r24", "r25", "gp",  "fp",  "sp",  "ra",  "ea",  "ba"
};
static const char csr_str[32][6] =
{
    "IE",    "IM",    "IP",    "ICC",   "DCC",   "CC",    "CFG",   "EBA",
    "DC",    "DEBA",  "CFG2",  "csr11", "csr12", "csr13", "JTX",   "JRX",
    "BP0",   "BP1",   "BP2",   "BP3",   "WP0",   "WP1",   "WP2",   "WP3",
    "csr24", "csr25", "csr26", "csr27", "csr28", "csr29", "csr30", "csr31"
};

static const vluint32_t lm32_sra_table[32] =
{
    0x00000000, 0x80000000, 0xC0000000, 0xE0000000,
    0xF0000000, 0xF8000000, 0xFC000000, 0xFE000000,
    0xFF000000, 0xFF800000, 0xFFC00000, 0xFFE00000,
    0xFFF00000, 0xFFF80000, 0xFFFC0000, 0xFFFE0000,
    0xFFFF0000, 0xFFFF8000, 0xFFFFC000, 0xFFFFE000,
    0xFFFFF000, 0xFFFFF800, 0xFFFFFC00, 0xFFFFFE00,
    0xFFFFFF00, 0xFFFFFF80, 0xFFFFFFC0, 0xFFFFFFE0,
    0xFFFFFFF0, 0xFFFFFFF8, 0xFFFFFFFC, 0xFFFFFFFE
};

#define GET_BIT(A,N)   (((A) >> N) & 1)
#define SRA_32(A,N)    (((A) & 0x80000000) ? ((A) >> (N)) | lm32_sra_table[(N)] : ((A) >> (N)))

#define XFER_NONE      ((vluint8_t)0)
#define XFER_LB        ((vluint8_t)1)
#define XFER_LBU       ((vluint8_t)2)
#define XFER_LH        ((vluint8_t)3)
#define XFER_LHU       ((vluint8_t)4)
#define XFER_LW        ((vluint8_t)5)
#define XFER_SB        ((vluint8_t)6)
#define XFER_SH        ((vluint8_t)7)
#define XFER_SW        ((vluint8_t)8)

#define RAISE_NONE     ((vluint8_t)0)
#define RAISE_RESET    ((vluint8_t)8)
#define RAISE_BREAK    ((vluint8_t)9)
#define RAISE_IBUS_ERR ((vluint8_t)10)
#define RAISE_WATCH    ((vluint8_t)11)
#define RAISE_DBUS_ERR ((vluint8_t)12)
#define RAISE_DIV_ZERO ((vluint8_t)13)
#define RAISE_IRQ_PEND ((vluint8_t)14)
#define RAISE_SYS_CALL ((vluint8_t)15)

// Constructor
LM32Trace::LM32Trace(vluint32_t reset_vect, vluint32_t except_base)
{
    // Initialize PC
    pc_reg    = reset_vect & 0xFFFFFFFC;
    // Clear registers
    for (int i = 0; i < 16; i++)
    {
        gp_regs[i] = (vluint32_t)0;
    }
    // File handle set to STDOUT
    tname[0]    = (char)0;
    tfh         = stdout;
    // Internal variables cleared
    dasm_buf[0] = (char)0;
    prev_clk    = (vluint8_t)0;
    except_nr   = RAISE_NONE;
    mem_xfer    = XFER_NONE;
    mem_mask    = (vluint8_t)0xF;
    mem_addr    = (vluint32_t)0x00000000;
    ie_reg      = (vluint32_t)0;
    im_reg      = (vluint32_t)0;
    ip_reg      = (vluint32_t)0;
    eba_reg     = except_base & 0xFFFFFF00;
    cc_reg      = (vluint32_t)4;
}

// Destructor
LM32Trace::~LM32Trace()
{
}

// Open trace file
int LM32Trace::open(const char *name)
{
    FILE *fh;
    
    // Close previous file
    this->close();

    // Complete the file name
    strncpy(tname, name, 246);
    strcat(tname, "_0000.trc");
    
    // Try to open the file for writing
    fh = fopen(tname, "w");
    if (fh)
    {
        // Success
        tfh = fh;
        return 0;
    }
    else
    {
        // Failure
        tname[0] = (char)0;
        return -1;
    }
}

// Open next trace file
int LM32Trace::openNext(void)
{
    FILE *fh;
    int len;

    // Close previous file
    this->close();

    // Get filename length
    len = strlen(tname);
    if (!len) return -1;
    
    // Increment file name
    if (tname[len-5] == '9')
    {
        tname[len-5] = '0';
        if (tname[len-6] == '9')
        {
            tname[len-6] = '0';
            if (tname[len-7] == '9')
            {
                tname[len-7] = '0';
                tname[len-8]++;
            }
            else
            {
                tname[len-7]++;
            }
        }
        else
        {
            tname[len-6]++;
        }
    }
    else
    {
        tname[len-5]++;
    }
    
    // Try to open the file for writing
    fh = fopen(tname, "w");
    if (fh)
    {
        // Success
        tfh = fh;
        return 0;
    }
    else
    {
        // Failure
        tname[0] = (char)0;
        return -1;
    }
}

// Close trace file
void LM32Trace::close(void)
{
    if (tfh != stdout)
    {
        fclose(tfh);
        tfh = stdout;
    }
}

// Dump trace
void LM32Trace::dump
(
    vluint64_t stamp,
    // Clock
    vluint8_t  clk,
    // Instruction fetch
    vluint8_t  i_rd_ack,
    vluint32_t i_address,
    vluint32_t i_rddata,
    // Data read/write
    vluint8_t  d_rd_ack,
    vluint8_t  d_wr_ack,
    vluint32_t d_address,
    vluint8_t  d_byteena,
    vluint32_t d_rddata,
    vluint32_t d_wrdata,
    // Interrupt Receiver
    vluint32_t inr_ir_irq,
    // Register write-back
    vluint8_t  wb_ena,
    vluint8_t  wb_idx,
    vluint32_t wb_data
)
{
    // Rising edge on clock
    if (clk && !prev_clk)
    {
        ip_reg = ip_reg | inr_ir_irq & im_reg;
        if (wb_ena)
        {
            if (wb_idx != reg_wb)
            {
                fprintf(tfh, "!!! WRITEBACK INDEX MISMATCH !!!\n");
                fprintf(tfh, "Verilog : %2d, C-Model : %2d\n", wb_idx, reg_wb);
            }
            else if (gp_regs[reg_wb] != wb_data)
            {
                fprintf(tfh, "!!! WRITEBACK DATA MISMATCH !!!\n");
                fprintf(tfh, "Verilog : %08X, C-Model : %08X\n", wb_data, gp_regs[reg_wb]);
            }
        }
        if (d_rd_ack)
        {
            fprintf(tfh, "Memory read @ $%08X : %08X\n", d_address, d_rddata);
            
            // Instruction simulation (memory/writeback)
            lm32_simu_rd(d_address, d_rddata);
        }
        if (d_wr_ack)
        {
            char buf[10];
            
            memcpy(buf + 6, (d_byteena & 1) ? uhex_to_str(d_wrdata >>  0, 2) : "$XX", 3);
            memcpy(buf + 4, (d_byteena & 2) ? uhex_to_str(d_wrdata >>  8, 2) : "$XX", 3);
            memcpy(buf + 2, (d_byteena & 4) ? uhex_to_str(d_wrdata >> 16, 2) : "$XX", 3);
            memcpy(buf + 0, (d_byteena & 8) ? uhex_to_str(d_wrdata >> 24, 2) : "$XX", 3);
            buf[9] = (char)0;
            
            fprintf(tfh, "Memory write @ $%08X : %s\n", d_address, buf);
            
            // Instruction simulation (memory)
            lm32_simu_wr(d_address, d_wrdata, d_byteena);
        }
        if (i_rd_ack)
        {
            char buf[80];
            
            // CPU registers
            fprintf(tfh, "R0 =%08X %08X %08X %08X %08X %08X %08X %08X\n",
                    gp_regs[ 0], gp_regs[ 1], gp_regs[ 2], gp_regs[ 3],
                    gp_regs[ 4], gp_regs[ 5], gp_regs[ 6], gp_regs[ 7]
                   );
            fprintf(tfh, "R8 =%08X %08X %08X %08X %08X %08X %08X %08X\n",
                    gp_regs[ 8], gp_regs[ 9], gp_regs[10], gp_regs[11],
                    gp_regs[12], gp_regs[13], gp_regs[14], gp_regs[15]
                   );
            fprintf(tfh, "R16=%08X %08X %08X %08X %08X %08X %08X %08X\n",
                    gp_regs[16], gp_regs[17], gp_regs[18], gp_regs[19],
                    gp_regs[20], gp_regs[21], gp_regs[22], gp_regs[23]
                   );
            fprintf(tfh, "R24=%08X %08X %08X %08X %08X %08X %08X %08X\n\n",
                    gp_regs[24], gp_regs[25], gp_regs[26], gp_regs[27],
                    gp_regs[28], gp_regs[29], gp_regs[30], gp_regs[31]
                   );
                   
            // Disassemble instruction being fetched
            lm32_dasm(buf, i_rddata, pc_reg);
            fprintf(tfh, "(%14llu ps) %08X : %08X %s\n", stamp, i_address, i_rddata, buf);
            
            // Instruction simulation (fetch/decode/execute/writeback)
            lm32_simu_if(i_address, i_rddata);
        }
    }
    prev_clk = clk;
}

// Disassemble one instruction
char LM32Trace::disasm(vluint32_t inst, vluint32_t pc, int idx)
{
    if (idx == 0)
    {
        memset(dasm_buf, 0, 32);
        lm32_dasm(dasm_buf, inst, pc);
    }
    return dasm_buf[idx & 31];
}

/******************************************************************************/
/** uhex_to_str()                                                            **/
/** ------------------------------------------------------------------------ **/
/** Convert an unsigned 32-bit value into a hexadecimal string               **/
/**   val : 32-bit value                                                     **/
/**   dig : number of hexadecimal digits (1 - 8)                             **/
/******************************************************************************/

char *LM32Trace::uhex_to_str(vluint32_t val, int dig)
{
    static char buf[12];
    char *p;
    
    dig <<= 2;
    p = buf;
    
    *p++ = '$';
    while (dig)
    {
        dig -= 4;
        // Convert one digit
        *p++ = hex_dig[(val >> dig) & 15];
    }
    *p = (char)0;
    
    return buf;
}

/******************************************************************************/
/** shex_to_str()                                                            **/
/** ------------------------------------------------------------------------ **/
/** Convert a signed 8/16/32-bit value into a hexadecimal string             **/
/**   val : 8/16/32-bit value                                                **/
/**   dig : number of hexadecimal digits (1 - 8)                             **/
/******************************************************************************/

char *LM32Trace::shex_to_str(vluint32_t val, int dig)
{
    static char buf[12];
    char *p;
    vluint32_t msk;
    
    // 8, 16 or 32
    dig <<= 2;
    p = buf;
    
    // 0x80, 0x8000 or 0x80000000
    msk = (vluint32_t)1 << (dig - 1);
    if (val & msk)
    {
        val = (~val) + 1;
        *p++ = '-';
    }
    
    *p++ = '$';
    while (dig)
    {
        dig -= 4;
        // Convert one digit
        *p++ = hex_dig[(val >> dig) & 15];
    }
    *p = (char)0;
    
    return buf;
}

void LM32Trace::lm32_dasm(char *buf, vluint32_t inst, vluint32_t pc)
{
    vluint8_t opc;
    vluint8_t rX;
    vluint8_t rY;
    vluint8_t rZ;
    vluint32_t imm5;
    vluint32_t imm11;
    vluint32_t imm16;
    vluint32_t imm26;
    
    opc   = (inst >> 26) & 0x3F;
    rX    = (inst >> 21) & 0x1F;
    rY    = (inst >> 16) & 0x1F;
    rZ    = (inst >> 11) & 0x1F;
    imm5  =  inst        & 0x1F;
    imm11 =  inst        & 0x7FF;
    imm16 =  inst        & 0xFFFF;
    imm26 = (inst <<  2) & 0xFFFFFFC;
    
    switch (opc)
    {
        /////////////////////////
        // I-Type instructions //
        /////////////////////////
        case OP_LBU:
        case OP_LB:
        case OP_LHU:
        case OP_LH:
        case OP_LW:
        {
            sprintf(buf, "%s %s,%s(%s)",
                    opc_str[opc],
                    reg_str[rY],
                    shex_to_str(imm16, 4),
                    reg_str[rX]
                   );
            break;
        }
        case OP_SB:
        case OP_SH:
        case OP_SW:
        {
            sprintf(buf, "%s %s(%s),%s",
                    opc_str[opc],
                    shex_to_str(imm16, 4),
                    reg_str[rX],
                    reg_str[rY]
                   );
            break;
        }
        case OP_ANDI:
        case OP_ANDHI:
        case OP_ORI:
        case OP_ORHI:
        case OP_NORI:
        case OP_XORI:
        case OP_XNORI:
        case OP_CMPGEUI:
        case OP_CMPGUI:
        {
            sprintf(buf, "%s %s,%s,#%s",
                    opc_str[opc],
                    reg_str[rY],
                    reg_str[rX],
                    uhex_to_str(imm16, 4)
                   );
            break;
        }
        case OP_ADDI:
        case OP_MULI:
        case OP_CMPEI:
        case OP_CMPGI:
        case OP_CMPGEI:
        case OP_CMPNEI:
        {
            sprintf(buf, "%s %s,%s,#%s",
                    opc_str[opc],
                    reg_str[rY],
                    reg_str[rX],
                    shex_to_str(imm16, 4)
                   );
            break;
        }
        case OP_BE:
        case OP_BG:
        case OP_BGE:
        case OP_BGEU:
        case OP_BGU:
        case OP_BNE:
        {
            if (imm16 & 0x8000) imm16 |= 0xFFFF0000;
            sprintf(buf, "%s %s,%s,%s",
                    opc_str[opc],
                    reg_str[rX],
                    reg_str[rY],
                    uhex_to_str(pc + (imm16 << 2), 8)
                   );
            break;
        }
        case OP_SLI:
        case OP_SRI:
        case OP_SRUI:
        {
            sprintf(buf, "%s %s,%s,#%s",
                    opc_str[opc],
                    reg_str[rY],
                    reg_str[rX],
                    uhex_to_str(imm5, 2)
                   );
            break;
        }
        /////////////////////////
        // J-Type instructions //
        /////////////////////////
        case OP_BI:
        case OP_CALLI:
        {
            if (imm26 & 0x08000000) imm26 |= 0xF0000000;
            sprintf(buf, "%s %s",
                    opc_str[opc],
                    uhex_to_str(pc + imm26, 8)
                   );
            break;
        }
        /////////////////////////
        // R-Type instructions //
        /////////////////////////
        case OP_USER:
        {
            sprintf(buf, "%s #%s,%s,%s,%s",
                    opc_str[opc],
                    uhex_to_str(imm11, 3),
                    reg_str[rZ],
                    reg_str[rX],
                    reg_str[rY]
                   );
            break;
        }
        case OP_B:
        {
            switch(rX)
            {
                case 29 : sprintf(buf, "ret");  break;
                case 30 : sprintf(buf, "eret"); break;
                case 31 : sprintf(buf, "bret"); break;
                default :
                    sprintf(buf, "%s %s",
                            opc_str[opc],
                            reg_str[rX]
                           );
            }
            break;
        }
        case OP_CALL:
        {
            sprintf(buf, "%s %s",
                    opc_str[opc],
                    reg_str[rX]
                   );
            break;
        }
        case OP_SEXTB:
        case OP_SEXTH:
        {
            sprintf(buf, "%s %s,%s",
                    opc_str[opc],
                    reg_str[rZ],
                    reg_str[rX]
                   );
            break;
        }
        case OP_WCSR:
        {
            sprintf(buf, "%s %s,%s",
                    opc_str[opc],
                    csr_str[rX],
                    reg_str[rY]
                   );
            break;
        }
        case OP_RCSR:
        {
            sprintf(buf, "%s %s,%s",
                    opc_str[opc],
                    reg_str[rZ],
                    csr_str[rX]
                   );
            break;
        }
        case OP_RAISE:
        {
            switch(imm5 & 7)
            {
                case 0  : sprintf(buf, "reset"); break;
                case 1  : sprintf(buf, "break"); break;
                case 6  : sprintf(buf, "irq");   break;
                case 7  : sprintf(buf, "scall"); break;
                default : sprintf(buf, "%s #%d",
                                  opc_str[opc],
                                  imm5 & 7
                                 );
            }
            break;
        }
        default:
        {
            sprintf(buf, "%s %s,%s,%s",
                    opc_str[opc],
                    reg_str[rZ],
                    reg_str[rX],
                    reg_str[rY]
                   );
            break;
        }
    }
}

void LM32Trace::lm32_simu_if(vluint32_t addr, vluint32_t inst)
{
    bool inc_pc = true;
    vluint8_t opc;
    vluint8_t rX;
    vluint8_t rY;
    vluint8_t rZ;
    vluint32_t imm5;
    vluint32_t imm11;
    vluint32_t uimm16;
    vluint32_t simm26;
    unsigned long eimm16;
    signed   long simm16;
    unsigned long ureg_X;
    signed   long sreg_X;
    unsigned long ureg_Y;
    signed   long sreg_Y;
    
    if (addr != pc_reg)
    {
        fprintf(tfh, "!!! INST ADDRESS MISMATCH !!!\n");
        fprintf(tfh, "Verilog : %08X, C-Model : %08X\n", addr, pc_reg);
    }
    
    opc    = (inst >> 26) & 0x3F;
    rX     = (inst >> 21) & 0x1F;
    rY     = (inst >> 16) & 0x1F;
    rZ     = (inst >> 11) & 0x1F;
    imm5   =  inst        & 0x1F;
    imm11  =  inst        & 0x7FF;
    uimm16 =  inst        & 0xFFFF;
    eimm16 = GET_BIT(uimm16,15) ? (unsigned long)(0xFFFF0000 | uimm16) : (unsigned long)uimm16;
    simm16 = (eimm16 & 0x80000000) ? -((eimm16 ^ 0xFFFFFFFF) + 1) : eimm16;
    simm26 = GET_BIT(inst,25) ? (inst << 2) | 0xF0000000 : (inst << 2)  & 0x0FFFFFFC;
    ureg_X = (unsigned long)gp_regs[rX];
    sreg_X = (ureg_X & 0x80000000) ? -((ureg_X ^ 0xFFFFFFFF) + 1) : ureg_X;
    ureg_Y = (unsigned long)gp_regs[rY];
    sreg_Y = (ureg_Y & 0x80000000) ? -((ureg_Y ^ 0xFFFFFFFF) + 1) : ureg_Y;
    
    pc_reg += 4;
    
    if ((opc == OP_CALL) || (opc == OP_CALLI))
    {
        reg_wb = 29;
    }
    else if (opc & 32)
    {
        // R-Type
        reg_wb = rZ;
    }
    else
    {
        // I-Type
        reg_wb = rY;
    }
    
    switch (opc)
    {
        // 0x00
        case OP_SRUI:
        {
            if (reg_wb) gp_regs[reg_wb] = ureg_X >> imm5;
            cc_reg += (6 + imm5);
            break;
        }
        // 0x01
        case OP_NORI:
        {
            if (reg_wb) gp_regs[reg_wb] = ~(ureg_X | uimm16);
            cc_reg += 4;
            break;
        }
        // 0x02
        case OP_MULI:
        {
            if (reg_wb) gp_regs[reg_wb] = sreg_X * simm16;
            cc_reg += 38;
            break;
        }
        // 0x03
        case OP_SH:
        {
            mem_addr = ureg_X + eimm16;
            if (mem_addr & 1)
            {
                except_nr = RAISE_DBUS_ERR;
                cc_reg += 9;
            }
            else
            {
                mem_mask = (vluint8_t)0xC >> (mem_addr & 2);
                mem_data = (ureg_Y & 0xFFFF) * 0x00010001;
                mem_xfer = XFER_SH;
                cc_reg += 5;
            }
            break;
        }
        // 0x04
        case OP_LB:
        {
            mem_addr = ureg_X + eimm16;
            mem_mask = (vluint8_t)0xF;
            mem_xfer = XFER_LB;
            cc_reg += 7;
            break;
        }
        // 0x05
        case OP_SRI:
        {
            if (reg_wb) gp_regs[reg_wb] = SRA_32(ureg_X, imm5);
            cc_reg += (6 + imm5);
            break;
        }
        // 0x06
        case OP_XORI:
        {
            if (reg_wb) gp_regs[reg_wb] = ureg_X ^ uimm16;
            cc_reg += 4;
            break;
        }
        // 0x07
        case OP_LH:
        {
            mem_addr = ureg_X + eimm16;
            if (mem_addr & 1)
            {
                except_nr = RAISE_DBUS_ERR;
                cc_reg += 9;
            }
            else
            {
                mem_mask = (vluint8_t)0xF;
                mem_xfer = XFER_LH;
                cc_reg += 7;
            }
            break;
        }
        // 0x08
        case OP_ANDI:
        {
            if (reg_wb) gp_regs[reg_wb] = ureg_X & uimm16;
            cc_reg += 4;
            break;
        }
        // 0x09
        case OP_XNORI:
        {
            if (reg_wb) gp_regs[reg_wb] = ~(ureg_X ^ uimm16);
            cc_reg += 4;
            break;
        }
        // 0x0A
        case OP_LW:
        {
            mem_addr = ureg_X + eimm16;
            if (mem_addr & 3)
            {
                except_nr = RAISE_DBUS_ERR;
                cc_reg += 9;
            }
            else
            {
                mem_mask = (vluint8_t)0xF;
                mem_xfer = XFER_LW;
                cc_reg += 6;
            }
            break;
        }
        // 0x0B
        case OP_LHU:
        {
            mem_addr = ureg_X + eimm16;
            if (mem_addr & 1)
            {
                except_nr = RAISE_DBUS_ERR;
                cc_reg += 9;
            }
            else
            {
                mem_mask = (vluint8_t)0xF;
                mem_xfer = XFER_LHU;
                cc_reg += 7;
            }
            break;
        }
        // 0x0C
        case OP_SB:
        {
            mem_addr = ureg_X + eimm16;
            mem_mask = (vluint8_t)0x8 >> (mem_addr & 3);
            mem_data = (ureg_Y & 0xFF) * 0x01010101;
            mem_xfer = XFER_SB;
            cc_reg += 5;
            break;
        }
        // 0x0D
        case OP_ADDI:
        {
            if (reg_wb) gp_regs[reg_wb] = ureg_X + eimm16;
            cc_reg += 4;
            break;
        }
        // 0x0E
        case OP_ORI:
        {
            if (reg_wb) gp_regs[reg_wb] = ureg_X | uimm16;
            cc_reg += 4;
            break;
        }
        // 0x0F
        case OP_SLI:
        {
            if (reg_wb) gp_regs[reg_wb] = ureg_X << imm5;
            cc_reg += (6 + imm5);
            break;
        }
        // 0x10
        case OP_LBU:
        {
            mem_addr = ureg_X + eimm16;
            mem_mask = (vluint8_t)0xF;
            mem_xfer = XFER_LBU;
            cc_reg += 7;
            break;
        }
        // 0x11
        case OP_BE:
        {
            if (ureg_X == ureg_Y)
            {
                pc_reg  = pc_reg - 4 + (eimm16 << 2);
                cc_reg += 5;
            }
            else
            {
                cc_reg += 4;
            }
            inc_pc = false;
            break;
        }
        // 0x12
        case OP_BG:
        {
            if (sreg_X > sreg_Y)
            {
                pc_reg  = pc_reg - 4 + (eimm16 << 2);
                cc_reg += 5;
            }
            else
            {
                cc_reg += 4;
            }
            inc_pc = false;
            break;
        }
        // 0x13
        case OP_BGE:
        {
            if (sreg_X >= sreg_Y)
            {
                pc_reg  = pc_reg - 4 + (eimm16 << 2);
                cc_reg += 5;
            }
            else
            {
                cc_reg += 4;
            }
            inc_pc = false;
            break;
        }
        // 0x14
        case OP_BGEU:
        {
            if (ureg_X >= ureg_Y)
            {
                pc_reg  = pc_reg - 4 + (eimm16 << 2);
                cc_reg += 5;
            }
            else
            {
                cc_reg += 4;
            }
            inc_pc = false;
            break;
        }
        // 0x15
        case OP_BGU:
        {
            if (ureg_X > ureg_Y)
            {
                pc_reg  = pc_reg - 4 + (eimm16 << 2);
                cc_reg += 5;
            }
            else
            {
                cc_reg += 4;
            }
            inc_pc = false;
            break;
        }
        // 0x16
        case OP_SW:
        {
            mem_addr = ureg_X + eimm16;
            if (mem_addr & 3)
            {
                except_nr = RAISE_DBUS_ERR;
                cc_reg += 9;
            }
            else
            {
                mem_mask = (vluint8_t)0xF;
                mem_data = ureg_Y;
                mem_xfer = XFER_SW;
                cc_reg += 5;
            }
            break;
        }
        // 0x17
        case OP_BNE:
        {
            if (ureg_X != ureg_Y)
            {
                pc_reg  = pc_reg - 4 + (eimm16 << 2);
                cc_reg += 5;
            }
            else
            {
                cc_reg += 4;
            }
            inc_pc = false;
            break;
        }
        // 0x18
        case OP_ANDHI:
        {
            if (reg_wb) gp_regs[reg_wb] = ureg_X & (uimm16 << 16);
            cc_reg += 4;
            break;
        }
        // 0x19
        case OP_CMPEI:
        {
            if (reg_wb) gp_regs[reg_wb] = (ureg_X == eimm16) ? 1 : 0;
            cc_reg += 4;
            break;
        }
        // 0x1A
        case OP_CMPGI:
        {
            if (reg_wb) gp_regs[reg_wb] = (sreg_X > simm16) ? 1 : 0;
            cc_reg += 4;
            break;
        }
        // 0x1B
        case OP_CMPGEI:
        {
            if (reg_wb) gp_regs[reg_wb] = (sreg_X >= simm16) ? 1 : 0;
            cc_reg += 4;
            break;
        }
        // 0x1C
        case OP_CMPGEUI:
        {
            if (reg_wb) gp_regs[reg_wb] = (ureg_X >= uimm16) ? 1 : 0;
            cc_reg += 4;
            break;
        }
        // 0x1D
        case OP_CMPGUI:
        {
            if (reg_wb) gp_regs[reg_wb] = (ureg_X > uimm16) ? 1 : 0;
            cc_reg += 4;
            break;
        }
        // 0x1E
        case OP_ORHI:
        {
            if (reg_wb) gp_regs[reg_wb] = ureg_X | (uimm16 << 16);
            cc_reg += 4;
            break;
        }
        // 0x1F
        case OP_CMPNEI:
        {
            if (reg_wb) gp_regs[reg_wb] = (ureg_X != eimm16) ? 1 : 0;
            cc_reg += 4;
            break;
        }
        // 0x20
        case OP_SRU:
        {
            if (reg_wb) gp_regs[reg_wb] = ureg_X >> (ureg_Y & 0x1F);
            cc_reg += (6 + (ureg_Y & 0x1F));
            break;
        }
        // 0x21
        case OP_NOR:
        {
            if (reg_wb) gp_regs[reg_wb] = ~(ureg_X | ureg_Y);
            cc_reg += 4;
            break;
        }
        // 0x22
        case OP_MUL:
        {
            if (reg_wb) gp_regs[reg_wb] = sreg_X * sreg_Y;
            cc_reg += 38;
            break;
        }
        // 0x23
        case OP_DIVU:
        {
            if (ureg_Y == 0)
            {
                except_nr = RAISE_DIV_ZERO;
                cc_reg += 9;
            }
            else
            {
                if (reg_wb) gp_regs[reg_wb] = ureg_X / ureg_Y;
                cc_reg += 38;
            }
            break;
        }
        // 0x24
        case OP_RCSR:
        {
            switch(rX)
            {
                // CSR IE
                case 0x00 : gp_regs[reg_wb] = ie_reg; break;
                // CSR IM
                case 0x01 : gp_regs[reg_wb] = im_reg; break;
                // CSR IP
                case 0x02 : gp_regs[reg_wb] = ip_reg; break;
                // CSR CC
                case 0x05 : gp_regs[reg_wb] = cc_reg; break;
                // CSR CFG
                case 0x06 : gp_regs[reg_wb] = 0x00020037; break;
                // CSR EBA
                case 0x07 : gp_regs[reg_wb] = eba_reg; break;
                // Unimplemented
                default : gp_regs[reg_wb] = 0;
            }
            cc_reg += 4;
            break;
        }
        // 0x25
        case OP_SR:
        {
            if (reg_wb) gp_regs[reg_wb] = SRA_32(ureg_X, ureg_Y & 0x1F);
            cc_reg += (6 + (ureg_Y & 0x1F));
            break;
        }
        // 0x26
        case OP_XOR:
        {
            if (reg_wb) gp_regs[reg_wb] = ureg_X ^ ureg_Y;
            cc_reg += 4;
            break;
        }
        // 0x27
        case OP_DIV:
        {
            if (ureg_Y == 0)
            {
                except_nr = RAISE_DIV_ZERO;
                cc_reg += 9;
            }
            else
            {
                if (reg_wb) gp_regs[reg_wb] = sreg_X / sreg_Y;
                cc_reg += 38;
            }
            break;
        }
        // 0x28
        case OP_AND:
        {
            if (reg_wb) gp_regs[reg_wb] = ureg_X & ureg_Y;
            cc_reg += 4;
            break;
        }
        // 0x29
        case OP_XNOR:
        {
            if (reg_wb) gp_regs[reg_wb] = ~(ureg_X ^ ureg_Y);
            cc_reg += 4;
            break;
        }
        // 0x2A
        case OP_2A:
        {
            break;
        }
        // 0x2B
        case OP_RAISE:
        {
            except_nr = 8 + imm5 & 7;
            cc_reg += 5;
            break;
        }
        // 0x2C
        case OP_SEXTB:
        {
            if (reg_wb) gp_regs[reg_wb] = (ureg_X & 0x80) ? ureg_X | 0xFFFFFF00 : ureg_X & 0xFF;
            cc_reg += 4;
            break;
        }
        // 0x2D
        case OP_ADD:
        {
            if (reg_wb) gp_regs[reg_wb] = ureg_X + ureg_Y;
            cc_reg += 4;
            break;
        }
        // 0x2E
        case OP_OR:
        {
            if (reg_wb) gp_regs[reg_wb] = ureg_X | ureg_Y;
            cc_reg += 4;
            break;
        }
        // 0x2F
        case OP_SL:
        {
            if (reg_wb) gp_regs[reg_wb] = ureg_X << (ureg_Y & 0x1F);
            cc_reg += (6 + (ureg_Y & 0x1F));
            break;
        }
        // 0x30
        case OP_B:
        {
            if (ureg_X & 3)
            {
                except_nr = RAISE_IBUS_ERR;
                cc_reg += 9;
            }
            else
            {
                if (rX == 31)
                {
                    // bret : IE = BIE
                    ie_reg = ((ie_reg & 0x4) >> 2) | (ie_reg & 0x2);
                }
                if (rX == 30)
                {
                    // eret : IE = EIE
                    ie_reg = ((ie_reg & 0x2) >> 1) | (ie_reg & 0x4);
                }
                pc_reg = ureg_X;
                cc_reg += 5;
            }
            inc_pc = false;
            break;
        }
        // 0x31
        case OP_MODU:
        {
            if (ureg_Y == 0)
            {
                except_nr = RAISE_DIV_ZERO;
                cc_reg += 9;
            }
            else
            {
                if (reg_wb) gp_regs[reg_wb] = ureg_X % ureg_Y;
                cc_reg += 38;
            }
            break;
        }
        // 0x32
        case OP_SUB:
        {
            if (reg_wb) gp_regs[reg_wb] = ureg_X - ureg_Y;
            cc_reg += 4;
            break;
        }
        // 0x33
        case OP_USER:
        {
            break;
        }
        // 0x34
        case OP_WCSR:
        {
            switch(rX)
            {
                // CSR IE
                case 0x00 : ie_reg  = ureg_Y & 0x00000007; break;
                // CSR IM
                case 0x01 : im_reg  = ureg_Y; break;
                // CSR IP
                case 0x02 : ip_reg  = ip_reg & ~ureg_Y; break;
                // CSR EBA
                case 0x07 : eba_reg = ureg_Y & 0xFFFFFF00; break;
                // Unimplemented
                default : ;
            }
            cc_reg += 4;
            break;
        }
        // 0x35
        case OP_MOD:
        {
            if (ureg_Y == 0)
            {
                except_nr = RAISE_DIV_ZERO;
                cc_reg += 9;
            }
            else
            {
                if (reg_wb) gp_regs[reg_wb] = sreg_X % sreg_Y;
                cc_reg += 38;
            }
            break;
        }
        // 0x36
        case OP_CALL:
        {
            if (ureg_X & 3)
            {
                except_nr = RAISE_IBUS_ERR;
                cc_reg += 9;
            }
            else
            {
                gp_regs[reg_wb] = pc_reg;
                pc_reg = ureg_X;
                cc_reg += 5;
            }
            inc_pc = false;
            break;
        }
        // 0x37
        case OP_SEXTH:
        {
            if (reg_wb) gp_regs[reg_wb] = (ureg_X & 0x8000) ? ureg_X | 0xFFFF0000 : ureg_X & 0xFFFF;
            cc_reg += 4;
            break;
        }
        // 0x38
        case OP_BI:
        {
            pc_reg  = pc_reg - 4 + simm26;
            cc_reg += 5;
            inc_pc = false;
            break;
        }
        // 0x39
        case OP_CMPE:
        {
            if (reg_wb) gp_regs[reg_wb] = (ureg_X == ureg_Y) ? 1 : 0;
            cc_reg += 4;
            break;
        }
        // 0x3A
        case OP_CMPG:
        {
            if (reg_wb) gp_regs[reg_wb] = (sreg_X > sreg_Y) ? 1 : 0;
            cc_reg += 4;
            break;
        }
        // 0x3B
        case OP_CMPGE:
        {
            if (reg_wb) gp_regs[reg_wb] = (sreg_X >= sreg_Y) ? 1 : 0;
            cc_reg += 4;
            break;
        }
        // 0x3C
        case OP_CMPGEU:
        {
            if (reg_wb) gp_regs[reg_wb] = (ureg_X >= ureg_Y) ? 1 : 0;
            cc_reg += 4;
            break;
        }
        // 0x3D
        case OP_CMPGU:
        {
            if (reg_wb) gp_regs[reg_wb] = (ureg_X > ureg_Y) ? 1 : 0;
            cc_reg += 4;
            break;
        }
        // 0x3E
        case OP_CALLI:
        {
            gp_regs[reg_wb] = pc_reg;
            pc_reg  = pc_reg - 4 + simm26;
            cc_reg += 5;
            inc_pc = false;
            break;
        }
        // 0x3F
        case OP_CMPNE:
        {
            if (reg_wb) gp_regs[reg_wb] = (ureg_X != ureg_Y) ? 1 : 0;
            cc_reg += 4;
            break;
        }
        default:
        {
            // Unknown instruction
        }
    }
    
    // Interrupts handling
    if ((ip_reg) && (ie_reg & 1) && (except_nr == RAISE_NONE) && (inc_pc))
    {
        except_nr = RAISE_IRQ_PEND;
    }
    
    // Exceptions handling
    if (except_nr)
    {
        if ((except_nr == RAISE_BREAK) || (except_nr == RAISE_WATCH))
        {
            reg_wb = 31; // ba
            ie_reg = ((ie_reg & 0x1) << 2) | (ie_reg & 0x2);
        }
        else
        {
            reg_wb = 30; // ea
            ie_reg = ((ie_reg & 0x1) << 1) | (ie_reg & 0x4);
        }
        gp_regs[reg_wb] = pc_reg;
        pc_reg = eba_reg + 32 * (except_nr & 7);
        except_nr = RAISE_NONE;
    }
}

void LM32Trace::lm32_simu_rd(vluint32_t addr, vluint32_t data)
{
    if (addr != (mem_addr & 0xFFFFFFFC))
    {
        fprintf(tfh, "!!! DATA ADDRESS MISMATCH !!!\n");
        fprintf(tfh, "Verilog : %08X, C-Model : %08X\n", addr, (mem_addr & 0xFFFFFFFC));
    }
    
    switch (mem_xfer)
    {
        case XFER_LB:
        {
            if (reg_wb)
            {
                switch (mem_addr & 3)
                {
                    case 0 : gp_regs[reg_wb] = (data >> 24) & 0xFF; break;
                    case 1 : gp_regs[reg_wb] = (data >> 16) & 0xFF; break;
                    case 2 : gp_regs[reg_wb] = (data >>  8) & 0xFF; break;
                    case 3 : gp_regs[reg_wb] = (data >>  0) & 0xFF; break;
                }
                if (GET_BIT(gp_regs[reg_wb],7)) gp_regs[reg_wb] |= 0xFFFFFF00;
            }
            break;
        }
        case XFER_LBU:
        {
            if (reg_wb)
            {
                switch (mem_addr & 3)
                {
                    case 0 : gp_regs[reg_wb] = (data >> 24) & 0xFF; break;
                    case 1 : gp_regs[reg_wb] = (data >> 16) & 0xFF; break;
                    case 2 : gp_regs[reg_wb] = (data >>  8) & 0xFF; break;
                    case 3 : gp_regs[reg_wb] = (data >>  0) & 0xFF; break;
                }
            }
            break;
        }
        case XFER_LH:
        {
            if (reg_wb)
            {
                switch (mem_addr & 2)
                {
                    case 0 : gp_regs[reg_wb] = (data >> 16) & 0xFFFF; break;
                    case 2 : gp_regs[reg_wb] = (data >>  0) & 0xFFFF; break;
                }
                if (GET_BIT(gp_regs[reg_wb],15)) gp_regs[reg_wb] |= 0xFFFF0000;
            }
            break;
        }
        case XFER_LHU:
        {
            if (reg_wb)
            {
                switch (mem_addr & 2)
                {
                    case 0 : gp_regs[reg_wb] = (data >> 16) & 0xFFFF; break;
                    case 2 : gp_regs[reg_wb] = (data >>  0) & 0xFFFF; break;
                }
            }
            break;
        }
        case XFER_LW:
        {
            if (reg_wb) gp_regs[reg_wb] = data;
            break;
        }
        default:
        {
            fprintf(tfh, "!!! DATA TRANSFER TYPE MISMATCH !!!\n");
        }
    }
    mem_xfer = XFER_NONE;
}

void LM32Trace::lm32_simu_wr(vluint32_t addr, vluint32_t data, vluint8_t mask)
{
    
    if (addr != (mem_addr & 0xFFFFFFFC))
    {
        fprintf(tfh, "!!! DATA ADDRESS MISMATCH !!!\n");
        fprintf(tfh, "Verilog : %08X, C-Model : %08X\n", addr, (mem_addr & 0xFFFFFFFC));
    }
    
    if (data != mem_data)
    {
        fprintf(tfh, "!!! DATA VALUE MISMATCH !!!\n");
        fprintf(tfh, "Verilog : %08X, C-Model : %08X\n", data, mem_data);
    }
    
    if (mask != mem_mask)
    {
        fprintf(tfh, "!!! DATA MASK MISMATCH !!!\n");
        fprintf(tfh, "Verilog : %1X, C-Model : %1X\n", mask, mem_mask);
    }
    mem_xfer = XFER_NONE;
}