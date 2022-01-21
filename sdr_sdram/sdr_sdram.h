// Copyright 2013-2022 Frederic Requin
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
// SDRAM C++ model:
// ----------------
//  - Based on the verilog model from Micron : "mt48lc8m16a2.v"
//  - Designed to work with "Verilator" tool (www.veripool.org)
//  - 8/16/32/64-bit data bus supported
//  - 4 or 8 banks
//  - Two memory layouts : interleaved banks or contiguous banks
//  - Sequential and interleaved bursts supported
//  - Binary images can be loaded to and saved from SDRAM
//  - Debug mode to trace every SDRAM access
//  - Endianness support for 16, 32 and 64-bit memories
//  - Direct read/write memory access to use with DPI shortcut in controller
//

#ifndef _SDR_SDRAM_H_
#define _SDR_SDRAM_H_

#include "verilated.h"

#ifdef _SDRAM_8_BANKS_
/* For simulation only !! */
#define SDRAM_NUM_BANKS        (8)
#define SDRAM_BIT_BANKS        (3)
#else
#define SDRAM_NUM_BANKS        (4)
#define SDRAM_BIT_BANKS        (2)
#endif /* _SDRAM_8_BANKS_ */

#define DQM_PIPE_DEPTH         (2)

#define FLAG_DATA_WIDTH_8      ((vluint8_t)0x00)
#define FLAG_DATA_WIDTH_16     ((vluint8_t)0x01)
#define FLAG_DATA_WIDTH_32     ((vluint8_t)0x03)
#define FLAG_DATA_WIDTH_64     ((vluint8_t)0x07)
#define FLAG_BANK_INTERLEAVING ((vluint8_t)0x08)
#define FLAG_BIG_ENDIAN        ((vluint8_t)0x10)
#define FLAG_RANDOM_FILLED     ((vluint8_t)0x20)

class SDRAM
{
    public:
        // Constructor and destructor
        SDRAM(vluint8_t log2_rows, vluint8_t log2_cols, vluint8_t flags, const char *logfile);
        ~SDRAM();
        // Methods :
        // ---------
        // Binary image load
        void load(const char *name, vluint32_t size,  vluint32_t addr);
        // Binary image save
        void save(const char *name, vluint32_t size,  vluint32_t addr);
        // Cycle evaluate
        void eval(vluint64_t ts,    vluint8_t clk,    vluint8_t  cke,
                  vluint8_t  cs_n,  vluint8_t ras_n,  vluint8_t  cas_n, vluint8_t we_n,
                  vluint8_t  ba,    vluint16_t addr,
                  vluint8_t  dqm,   vluint64_t dq_in, vluint64_t &dq_out);
        // Direct memory read access
        vluint8_t  read_byte(vluint32_t addr);
        vluint16_t read_word(vluint32_t addr);
        vluint32_t read_long(vluint32_t addr);
        vluint64_t read_quad(vluint32_t addr);
        // Direct memory write access
        void write_byte(vluint32_t addr, vluint8_t  data);
        void write_word(vluint32_t addr, vluint16_t data);
        void write_long(vluint32_t addr, vluint32_t data);
        void write_quad(vluint32_t addr, vluint64_t data);
        // Memory size (in bytes)
        vluint32_t mem_size;
    private:
        // Special type to access data byte lanes
        typedef union
        {
            vluint8_t  u8[8];
            vluint64_t u64;
        } dq_lane_t;
        // Special type to access command pipeline
        typedef union
        {
            vluint8_t  u8[4];
            vluint32_t pipe;
        } pipe_u8_t;
        typedef union
        {
            vluint16_t u16[4];
            vluint64_t pipe;
        } pipe_u16_t;
        // Memory reading functions (to speedup access)
        vluint8_t  (SDRAM::*read_u8_priv)(vluint32_t);
        vluint16_t (SDRAM::*read_u16_priv)(vluint32_t);
        vluint32_t (SDRAM::*read_u32_priv)(vluint32_t);
        vluint64_t (SDRAM::*read_u64_priv)(vluint32_t);
        vluint8_t  read_u8_i(vluint32_t addr);
        vluint16_t read_u16_i_be(vluint32_t addr);
        vluint16_t read_u16_i_le(vluint32_t addr);
        vluint32_t read_u32_i_be(vluint32_t addr);
        vluint32_t read_u32_i_le(vluint32_t addr);
        vluint64_t read_u64_i_be(vluint32_t addr);
        vluint64_t read_u64_i_le(vluint32_t addr);
        vluint8_t  read_u8_c(vluint32_t addr);
        vluint16_t read_u16_c_be(vluint32_t addr);
        vluint16_t read_u16_c_le(vluint32_t addr);
        vluint32_t read_u32_c_be(vluint32_t addr);
        vluint32_t read_u32_c_le(vluint32_t addr);
        vluint64_t read_u64_c_be(vluint32_t addr);
        vluint64_t read_u64_c_le(vluint32_t addr);
        // Memory writing functions (to speedup access)
        void (SDRAM::*write_u8_priv)(vluint32_t, vluint8_t);
        void (SDRAM::*write_u16_priv)(vluint32_t, vluint16_t);
        void (SDRAM::*write_u32_priv)(vluint32_t, vluint32_t);
        void (SDRAM::*write_u64_priv)(vluint32_t, vluint64_t);
        void write_u8_i(vluint32_t addr, vluint8_t data);
        void write_u16_i_be(vluint32_t addr, vluint16_t data);
        void write_u16_i_le(vluint32_t addr, vluint16_t data);
        void write_u32_i_be(vluint32_t addr, vluint32_t data);
        void write_u32_i_le(vluint32_t addr, vluint32_t data);
        void write_u64_i_be(vluint32_t addr, vluint64_t data);
        void write_u64_i_le(vluint32_t addr, vluint64_t data);
        void write_u8_c(vluint32_t addr, vluint8_t data);
        void write_u16_c_be(vluint32_t addr, vluint16_t data);
        void write_u16_c_le(vluint32_t addr, vluint16_t data);
        void write_u32_c_be(vluint32_t addr, vluint32_t data);
        void write_u32_c_le(vluint32_t addr, vluint32_t data);
        void write_u64_c_be(vluint32_t addr, vluint64_t data);
        void write_u64_c_le(vluint32_t addr, vluint64_t data);
        // SDRAM capacity
        int        bus_mask;                     // Data bus width (bytes - 1)
        int        bus_log2;                     // Data bus width (log2(bytes))
        int        num_rows;                     // Number of rows
        int        num_cols;                     // Number of columns
        int        bit_rows;                     // Number of rows (log 2)
        int        bit_cols;                     // Number of columns (log 2)
        vluint32_t mask_bank;                    // Bit mask for banks
        vluint32_t mask_rows;                    // Bit mask for rows
        vluint32_t mask_cols;                    // Bit mask for columns
        // Memory arrays
        vluint8_t  *array_u8[SDRAM_NUM_BANKS];   // 8-bit access
        vluint16_t *array_u16[SDRAM_NUM_BANKS];  // 16-bit access
        vluint32_t *array_u32[SDRAM_NUM_BANKS];  // 32-bit access
        vluint64_t *array_u64[SDRAM_NUM_BANKS];  // 64-bit access
        // Mode register
        int        cas_lat;                      // CAS latency (2 or 3)
        int        bst_len_rd;                   // Burst length during read
        int        bst_msk_rd;
        int        bst_len_wr;                   // Burst length during write
        int        bst_msk_wr;
        vluint8_t  bst_type;                     // Burst type
        // Debug mode
        bool       dbg_on;
        // Special memory flags
        vluint8_t  mem_flags;
        // Internal variables
        vluint8_t  prev_clk;                     // Previous clock state
        pipe_u8_t  cmd_pipe;                     // Command pipeline
        pipe_u16_t col_pipe;                     // Column address pipeline
        pipe_u8_t  ba_pipe;                      // Bank address pipeline
        pipe_u8_t  bap_pipe;                     // Bank precharge pipeline
        pipe_u16_t a10_pipe;                     // A[10] wire pipeline
        vluint8_t  dqm_pipe[DQM_PIPE_DEPTH];     // DQM pipeline (for read)
        vluint8_t  row_act[SDRAM_NUM_BANKS];     // Bank activate
        vluint8_t  row_pre[SDRAM_NUM_BANKS];     // Bank precharge
        int        row_addr[SDRAM_NUM_BANKS];    // Row address during activate
        vluint16_t ap_bank[SDRAM_NUM_BANKS];     // Bank being auto-precharged
        int        bank;                         // Current bank during read/write
        int        row;                          // Current row during read/write
        int        col;                          // Current column during read/write
        int        bst_ctr_rd;                   // Burst counter (read)
        int        bst_ctr_wr;                   // Burst counter (write)
        // Log file
        FILE      *fh_log;
        char      *log_buf;
        int        log_size;
};

#endif /* _SDR_SDRAM_H_ */
