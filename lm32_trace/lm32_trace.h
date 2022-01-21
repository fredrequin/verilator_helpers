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

#ifndef _LM32_TRACE_H_
#define _LM32_TRACE_H_

#include "verilated.h"
#include <stdlib.h>
#include <stdio.h>

class LM32Trace
{
    public:
        // Constructor and destructor
        LM32Trace(vluint32_t reset_vect, vluint32_t except_base);
        ~LM32Trace();
        // Methods
        int  open(const char *name);
        int  openNext(void);
        void close(void);
        void dump(vluint64_t stamp,     vluint8_t  clk,
                  vluint8_t  i_rd_ack,  vluint32_t i_address, vluint32_t i_rddata,
                  vluint8_t  d_rd_ack,  vluint8_t  d_wr_ack,  vluint32_t d_address,
                  vluint8_t  d_byteena, vluint32_t d_rddata,  vluint32_t d_wrdata,
                  vluint32_t inr_ir_irq,
                  vluint8_t  wb_ena,    vluint8_t  wb_idx,    vluint32_t wb_data);
        char disasm(vluint32_t inst, vluint32_t pc, int idx);
    private:
        // Utility functions
        char       *uhex_to_str(vluint32_t val, int dig);
        char       *shex_to_str(vluint32_t val, int dig);
        // Mico32 disassembler
        void        lm32_dasm(char *buf, vluint32_t inst, vluint32_t pc);
        // Mico32 simulator
        void        lm32_simu_if(vluint32_t addr, vluint32_t inst);
        void        lm32_simu_rd(vluint32_t addr, vluint32_t data);
        void        lm32_simu_wr(vluint32_t addr, vluint32_t data, vluint8_t mask);
        // General purpose registers
        vluint32_t  gp_regs[32];
        // Program counter
        vluint32_t  pc_reg;
        // Interrupt registers
        vluint32_t  ie_reg;
        vluint32_t  im_reg;
        vluint32_t  ip_reg;
        // Exception base address
        vluint32_t  eba_reg;
        // Cycle counter register
        vluint32_t  cc_reg;
        // Disassembly buffer
        char        dasm_buf[32];
        // Trace file handle
        char        tname[256];
        FILE       *tfh;
        // Previous clock state
        vluint8_t   prev_clk;
        // Register writeback
        vluint8_t   reg_wb;
        // Exception number
        vluint8_t   except_nr;
        // Transfer type (load/store)
        vluint8_t   mem_xfer;
        // Bytes masking (load/store)
        vluint8_t   mem_mask;
        // Memory address (load/store)
        vluint32_t  mem_addr;
        // Memory data (store)
        vluint32_t  mem_data;
};

#endif /* _LM32_TRACE_H_ */