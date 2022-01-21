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

#include "verilated.h"
#include "sdr_sdram.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// SDRAM commands
#define CMD_LMR  ((vluint8_t)0)
#define CMD_REF  ((vluint8_t)1)
#define CMD_PRE  ((vluint8_t)2)
#define CMD_ACT  ((vluint8_t)3)
#define CMD_WR   ((vluint8_t)4)
#define CMD_RD   ((vluint8_t)5)
#define CMD_BST  ((vluint8_t)6)
#define CMD_NOP  ((vluint8_t)7)

// Data lanes
#define DATA_MSB ((vluint8_t)0x01)
#define DATA_MSW ((vluint8_t)0x02)
#define DATA_MSL ((vluint8_t)0x04)

// DQ data masking with DQM pins (64-bit bus)
static const vluint64_t c_dqm_mask[256] =
{
    0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFF00ULL, 0xFFFFFFFFFFFF00FFULL, 0xFFFFFFFFFFFF0000ULL,
    0xFFFFFFFFFF00FFFFULL, 0xFFFFFFFFFF00FF00ULL, 0xFFFFFFFFFF0000FFULL, 0xFFFFFFFFFF000000ULL,
    0xFFFFFFFF00FFFFFFULL, 0xFFFFFFFF00FFFF00ULL, 0xFFFFFFFF00FF00FFULL, 0xFFFFFFFF00FF0000ULL,
    0xFFFFFFFF0000FFFFULL, 0xFFFFFFFF0000FF00ULL, 0xFFFFFFFF000000FFULL, 0xFFFFFFFF00000000ULL,
    0xFFFFFF00FFFFFFFFULL, 0xFFFFFF00FFFFFF00ULL, 0xFFFFFF00FFFF00FFULL, 0xFFFFFF00FFFF0000ULL,
    0xFFFFFF00FF00FFFFULL, 0xFFFFFF00FF00FF00ULL, 0xFFFFFF00FF0000FFULL, 0xFFFFFF00FF000000ULL,
    0xFFFFFF0000FFFFFFULL, 0xFFFFFF0000FFFF00ULL, 0xFFFFFF0000FF00FFULL, 0xFFFFFF0000FF0000ULL,
    0xFFFFFF000000FFFFULL, 0xFFFFFF000000FF00ULL, 0xFFFFFF00000000FFULL, 0xFFFFFF0000000000ULL,
    0xFFFF00FFFFFFFFFFULL, 0xFFFF00FFFFFFFF00ULL, 0xFFFF00FFFFFF00FFULL, 0xFFFF00FFFFFF0000ULL,
    0xFFFF00FFFF00FFFFULL, 0xFFFF00FFFF00FF00ULL, 0xFFFF00FFFF0000FFULL, 0xFFFF00FFFF000000ULL,
    0xFFFF00FF00FFFFFFULL, 0xFFFF00FF00FFFF00ULL, 0xFFFF00FF00FF00FFULL, 0xFFFF00FF00FF0000ULL,
    0xFFFF00FF0000FFFFULL, 0xFFFF00FF0000FF00ULL, 0xFFFF00FF000000FFULL, 0xFFFF00FF00000000ULL,
    0xFFFF0000FFFFFFFFULL, 0xFFFF0000FFFFFF00ULL, 0xFFFF0000FFFF00FFULL, 0xFFFF0000FFFF0000ULL,
    0xFFFF0000FF00FFFFULL, 0xFFFF0000FF00FF00ULL, 0xFFFF0000FF0000FFULL, 0xFFFF0000FF000000ULL,
    0xFFFF000000FFFFFFULL, 0xFFFF000000FFFF00ULL, 0xFFFF000000FF00FFULL, 0xFFFF000000FF0000ULL,
    0xFFFF00000000FFFFULL, 0xFFFF00000000FF00ULL, 0xFFFF0000000000FFULL, 0xFFFF000000000000ULL,
    0xFF00FFFFFFFFFFFFULL, 0xFF00FFFFFFFFFF00ULL, 0xFF00FFFFFFFF00FFULL, 0xFF00FFFFFFFF0000ULL,
    0xFF00FFFFFF00FFFFULL, 0xFF00FFFFFF00FF00ULL, 0xFF00FFFFFF0000FFULL, 0xFF00FFFFFF000000ULL,
    0xFF00FFFF00FFFFFFULL, 0xFF00FFFF00FFFF00ULL, 0xFF00FFFF00FF00FFULL, 0xFF00FFFF00FF0000ULL,
    0xFF00FFFF0000FFFFULL, 0xFF00FFFF0000FF00ULL, 0xFF00FFFF000000FFULL, 0xFF00FFFF00000000ULL,
    0xFF00FF00FFFFFFFFULL, 0xFF00FF00FFFFFF00ULL, 0xFF00FF00FFFF00FFULL, 0xFF00FF00FFFF0000ULL,
    0xFF00FF00FF00FFFFULL, 0xFF00FF00FF00FF00ULL, 0xFF00FF00FF0000FFULL, 0xFF00FF00FF000000ULL,
    0xFF00FF0000FFFFFFULL, 0xFF00FF0000FFFF00ULL, 0xFF00FF0000FF00FFULL, 0xFF00FF0000FF0000ULL,
    0xFF00FF000000FFFFULL, 0xFF00FF000000FF00ULL, 0xFF00FF00000000FFULL, 0xFF00FF0000000000ULL,
    0xFF0000FFFFFFFFFFULL, 0xFF0000FFFFFFFF00ULL, 0xFF0000FFFFFF00FFULL, 0xFF0000FFFFFF0000ULL,
    0xFF0000FFFF00FFFFULL, 0xFF0000FFFF00FF00ULL, 0xFF0000FFFF0000FFULL, 0xFF0000FFFF000000ULL,
    0xFF0000FF00FFFFFFULL, 0xFF0000FF00FFFF00ULL, 0xFF0000FF00FF00FFULL, 0xFF0000FF00FF0000ULL,
    0xFF0000FF0000FFFFULL, 0xFF0000FF0000FF00ULL, 0xFF0000FF000000FFULL, 0xFF0000FF00000000ULL,
    0xFF000000FFFFFFFFULL, 0xFF000000FFFFFF00ULL, 0xFF000000FFFF00FFULL, 0xFF000000FFFF0000ULL,
    0xFF000000FF00FFFFULL, 0xFF000000FF00FF00ULL, 0xFF000000FF0000FFULL, 0xFF000000FF000000ULL,
    0xFF00000000FFFFFFULL, 0xFF00000000FFFF00ULL, 0xFF00000000FF00FFULL, 0xFF00000000FF0000ULL,
    0xFF0000000000FFFFULL, 0xFF0000000000FF00ULL, 0xFF000000000000FFULL, 0xFF00000000000000ULL,
    0x00FFFFFFFFFFFFFFULL, 0x00FFFFFFFFFFFF00ULL, 0x00FFFFFFFFFF00FFULL, 0x00FFFFFFFFFF0000ULL,
    0x00FFFFFFFF00FFFFULL, 0x00FFFFFFFF00FF00ULL, 0x00FFFFFFFF0000FFULL, 0x00FFFFFFFF000000ULL,
    0x00FFFFFF00FFFFFFULL, 0x00FFFFFF00FFFF00ULL, 0x00FFFFFF00FF00FFULL, 0x00FFFFFF00FF0000ULL,
    0x00FFFFFF0000FFFFULL, 0x00FFFFFF0000FF00ULL, 0x00FFFFFF000000FFULL, 0x00FFFFFF00000000ULL,
    0x00FFFF00FFFFFFFFULL, 0x00FFFF00FFFFFF00ULL, 0x00FFFF00FFFF00FFULL, 0x00FFFF00FFFF0000ULL,
    0x00FFFF00FF00FFFFULL, 0x00FFFF00FF00FF00ULL, 0x00FFFF00FF0000FFULL, 0x00FFFF00FF000000ULL,
    0x00FFFF0000FFFFFFULL, 0x00FFFF0000FFFF00ULL, 0x00FFFF0000FF00FFULL, 0x00FFFF0000FF0000ULL,
    0x00FFFF000000FFFFULL, 0x00FFFF000000FF00ULL, 0x00FFFF00000000FFULL, 0x00FFFF0000000000ULL,
    0x00FF00FFFFFFFFFFULL, 0x00FF00FFFFFFFF00ULL, 0x00FF00FFFFFF00FFULL, 0x00FF00FFFFFF0000ULL,
    0x00FF00FFFF00FFFFULL, 0x00FF00FFFF00FF00ULL, 0x00FF00FFFF0000FFULL, 0x00FF00FFFF000000ULL,
    0x00FF00FF00FFFFFFULL, 0x00FF00FF00FFFF00ULL, 0x00FF00FF00FF00FFULL, 0x00FF00FF00FF0000ULL,
    0x00FF00FF0000FFFFULL, 0x00FF00FF0000FF00ULL, 0x00FF00FF000000FFULL, 0x00FF00FF00000000ULL,
    0x00FF0000FFFFFFFFULL, 0x00FF0000FFFFFF00ULL, 0x00FF0000FFFF00FFULL, 0x00FF0000FFFF0000ULL,
    0x00FF0000FF00FFFFULL, 0x00FF0000FF00FF00ULL, 0x00FF0000FF0000FFULL, 0x00FF0000FF000000ULL,
    0x00FF000000FFFFFFULL, 0x00FF000000FFFF00ULL, 0x00FF000000FF00FFULL, 0x00FF000000FF0000ULL,
    0x00FF00000000FFFFULL, 0x00FF00000000FF00ULL, 0x00FF0000000000FFULL, 0x00FF000000000000ULL,
    0x0000FFFFFFFFFFFFULL, 0x0000FFFFFFFFFF00ULL, 0x0000FFFFFFFF00FFULL, 0x0000FFFFFFFF0000ULL,
    0x0000FFFFFF00FFFFULL, 0x0000FFFFFF00FF00ULL, 0x0000FFFFFF0000FFULL, 0x0000FFFFFF000000ULL,
    0x0000FFFF00FFFFFFULL, 0x0000FFFF00FFFF00ULL, 0x0000FFFF00FF00FFULL, 0x0000FFFF00FF0000ULL,
    0x0000FFFF0000FFFFULL, 0x0000FFFF0000FF00ULL, 0x0000FFFF000000FFULL, 0x0000FFFF00000000ULL,
    0x0000FF00FFFFFFFFULL, 0x0000FF00FFFFFF00ULL, 0x0000FF00FFFF00FFULL, 0x0000FF00FFFF0000ULL,
    0x0000FF00FF00FFFFULL, 0x0000FF00FF00FF00ULL, 0x0000FF00FF0000FFULL, 0x0000FF00FF000000ULL,
    0x0000FF0000FFFFFFULL, 0x0000FF0000FFFF00ULL, 0x0000FF0000FF00FFULL, 0x0000FF0000FF0000ULL,
    0x0000FF000000FFFFULL, 0x0000FF000000FF00ULL, 0x0000FF00000000FFULL, 0x0000FF0000000000ULL,
    0x000000FFFFFFFFFFULL, 0x000000FFFFFFFF00ULL, 0x000000FFFFFF00FFULL, 0x000000FFFFFF0000ULL,
    0x000000FFFF00FFFFULL, 0x000000FFFF00FF00ULL, 0x000000FFFF0000FFULL, 0x000000FFFF000000ULL,
    0x000000FF00FFFFFFULL, 0x000000FF00FFFF00ULL, 0x000000FF00FF00FFULL, 0x000000FF00FF0000ULL,
    0x000000FF0000FFFFULL, 0x000000FF0000FF00ULL, 0x000000FF000000FFULL, 0x000000FF00000000ULL,
    0x00000000FFFFFFFFULL, 0x00000000FFFFFF00ULL, 0x00000000FFFF00FFULL, 0x00000000FFFF0000ULL,
    0x00000000FF00FFFFULL, 0x00000000FF00FF00ULL, 0x00000000FF0000FFULL, 0x00000000FF000000ULL,
    0x0000000000FFFFFFULL, 0x0000000000FFFF00ULL, 0x0000000000FF00FFULL, 0x0000000000FF0000ULL,
    0x000000000000FFFFULL, 0x000000000000FF00ULL, 0x00000000000000FFULL, 0x0000000000000000ULL,
};

// Column counter, interleaved bursts
static const int c_col_int[8][8] =
{
//  ctr  1  2  3  4  5  6  7
    { 0, 1, 3, 1, 7, 1, 3, 1 }, // col = 0
    { 0, 0, 2, 0, 6, 0, 2, 0 }, // col = 1
    { 0, 3, 1, 3, 5, 3, 1, 3 }, // col = 2
    { 0, 2, 0, 2, 4, 2, 0, 2 }, // col = 3
    { 0, 5, 7, 5, 3, 5, 7, 5 }, // col = 4
    { 0, 4, 6, 4, 2, 4, 6, 4 }, // col = 5
    { 0, 7, 5, 7, 1, 7, 5, 7 }, // col = 6
    { 0, 6, 4, 6, 0, 6, 4, 6 }  // col = 7
};

// Constructor
SDRAM::SDRAM(vluint8_t log2_rows, vluint8_t log2_cols, vluint8_t flags, const char *logfile)
{
    int bnk_size;
    // SDRAM capacity initialized
    bus_mask    =  flags & (DATA_MSB | DATA_MSW | DATA_MSL);
    bus_log2    = (flags & DATA_MSB) ? 1 : 0;
    bus_log2    = (flags & DATA_MSW) ? 2 : bus_log2;
    bus_log2    = (flags & DATA_MSL) ? 3 : bus_log2;
    bit_rows    = (int)log2_rows;
    bit_cols    = (int)log2_cols;
    num_rows    = (int)1 << log2_rows;
    num_cols    = (int)1 << log2_cols;
    mask_rows   = (vluint32_t)(num_rows - 1) << log2_cols;
    mask_bank   = (vluint32_t)(SDRAM_NUM_BANKS - 1) << log2_cols;
    mask_cols   = (vluint32_t)(num_cols - 1);
    if (flags & FLAG_BANK_INTERLEAVING)
    {
        // Banks are interleaved
        // |      rows       |  banks  |     columns     |
        // |<-- log2_rows -->|<-- 2 -->|<-- log2_cols -->|
        mask_rows <<= SDRAM_BIT_BANKS;
    }
    else
    {
        // Banks are contiguous
        // |  banks  |      rows       |     columns     |
        // |<-- 2 -->|<-- log2_rows -->|<-- log2_cols -->|
        mask_bank <<= log2_rows;
    }
    // Bank size (in bytes)
    bnk_size = (int)1 << (log2_rows + log2_cols + bus_log2);
    // Memory size (in bytes)
    mem_size = bnk_size << SDRAM_BIT_BANKS;
    // Init message
    printf("Instantiating %d MB SDRAM : %d banks x %d rows x %d cols x %d bits\n",
            mem_size >> 20, SDRAM_NUM_BANKS, num_rows, num_cols, 8 << bus_log2);
    // Memory read functions
    switch (flags & (FLAG_BANK_INTERLEAVING | FLAG_BIG_ENDIAN))
    {
        // Little endian, contiguous banks
        case 0x00 :
        {
            read_u8_priv   = &SDRAM::read_u8_c;
            read_u16_priv  = &SDRAM::read_u16_c_le;
            read_u32_priv  = &SDRAM::read_u32_c_le;
            read_u64_priv  = &SDRAM::read_u64_c_le;
            write_u8_priv  = &SDRAM::write_u8_c;
            write_u16_priv = &SDRAM::write_u16_c_le;
            write_u32_priv = &SDRAM::write_u32_c_le;
            write_u64_priv = &SDRAM::write_u64_c_le;
            break;
        }
        // Little endian, interleaved banks
        case 0x08 :
        {
            read_u8_priv   = &SDRAM::read_u8_i;
            read_u16_priv  = &SDRAM::read_u16_i_le;
            read_u32_priv  = &SDRAM::read_u32_i_le;
            read_u64_priv  = &SDRAM::read_u64_i_le;
            write_u8_priv  = &SDRAM::write_u8_i;
            write_u16_priv = &SDRAM::write_u16_i_le;
            write_u32_priv = &SDRAM::write_u32_i_le;
            write_u64_priv = &SDRAM::write_u64_i_le;
            break;
        }
        // Big endian, contiguous banks
        case 0x10 :
        {
            read_u8_priv   = &SDRAM::read_u8_c;
            read_u16_priv  = &SDRAM::read_u16_c_be;
            read_u32_priv  = &SDRAM::read_u32_c_be;
            read_u64_priv  = &SDRAM::read_u64_c_be;
            write_u8_priv  = &SDRAM::write_u8_c;
            write_u16_priv = &SDRAM::write_u16_c_be;
            write_u32_priv = &SDRAM::write_u32_c_be;
            write_u64_priv = &SDRAM::write_u64_c_be;
            break;
        }
        // Big endian, interleaved banks
        case 0x18 :
        {
            read_u8_priv   = &SDRAM::read_u8_i;
            read_u16_priv  = &SDRAM::read_u16_i_be;
            read_u32_priv  = &SDRAM::read_u32_i_be;
            read_u64_priv  = &SDRAM::read_u64_i_be;
            write_u8_priv  = &SDRAM::write_u8_i;
            write_u16_priv = &SDRAM::write_u16_i_be;
            write_u32_priv = &SDRAM::write_u32_i_be;
            write_u64_priv = &SDRAM::write_u64_i_be;
            break;
        }
    }
    
    // debug mode
    if (logfile)
    {
        fh_log   = fopen(logfile, "w");
        log_buf  = new char[2048];
        log_size = 0;
        if ((fh_log) && (log_buf))
        {
            printf("SDRAM log file \"%s\" created\n", logfile);
            dbg_on = true;
        }
        else
        {
            dbg_on = false;
        }
    }
    else
    {
        fh_log   = (FILE *)NULL;
        log_buf  = (char *)NULL;
        log_size = 0;
        dbg_on   = false;
    }
    
    // special flags
    mem_flags     = flags;
    
    // mode register cleared
    cas_lat       = 0;
    bst_len_rd    = (int)0;
    bst_msk_rd    = (int)0;
    bst_len_wr    = (int)0;
    bst_msk_wr    = (int)0;
    bst_type      = (vluint8_t)0;
    
    // internal variables cleared
    prev_clk      = (vluint8_t)0;
    cmd_pipe.pipe = (vluint32_t)0x07070707; // CMD_NOP x 4
    col_pipe.pipe = (vluint64_t)0;
    ba_pipe.pipe  = (vluint32_t)0;
    bap_pipe.pipe = (vluint32_t)0;
    a10_pipe.pipe = (vluint64_t)0;
    dqm_pipe[0]   = (vluint8_t)0;
    dqm_pipe[1]   = (vluint8_t)0;
    for (int i = 0; i < SDRAM_NUM_BANKS; i++)
    {
        row_act[i]  = (vluint8_t)1;
        row_pre[i]  = (vluint8_t)0;
        row_addr[i] = (int)0;
        ap_bank[i]  = (vluint16_t)0;
    }
    bank        = (int)0;
    row         = (int)0;
    col         = (int)0;
    bst_ctr_rd  = (int)0;
    bst_ctr_wr  = (int)0;

    // one array per bank (4 arrays)
    for (int b = 0; b < SDRAM_NUM_BANKS; b++) // bank
    {
        array_u64[b] = (vluint64_t *)new vluint64_t[bnk_size/sizeof(vluint64_t)];
        array_u32[b] = (vluint32_t *)array_u64[b];
        array_u16[b] = (vluint16_t *)array_u64[b];
        array_u8[b]  = (vluint8_t *)array_u64[b];
    }
    
    if (flags & FLAG_RANDOM_FILLED)
    {
        // fill the arrays with random numbers
        srand (time (NULL));
        for (int b = 0; b < SDRAM_NUM_BANKS; b++) // bank
        {
            for (int a = 0; a < bnk_size; a++) // array
            {
                array_u8[b][a] = (vluint8_t)rand() & 0xFF;
            }
        }
    }
    else
    {
        // clear the arrays
        for (int b = 0; b < SDRAM_NUM_BANKS; b++) // bank
        {
            memset((void *)array_u8[b], 0, bnk_size);
        }
    }
}

// Destructor
SDRAM::~SDRAM()
{
    // free the memory
    for (int b = 0; b < SDRAM_NUM_BANKS; b++) // bank
    {
        array_u64[b] = (vluint64_t *)NULL;
        array_u32[b] = (vluint32_t *)NULL;
        array_u16[b] = (vluint16_t *)NULL;
        delete[] array_u8[b];
    }
}

// Binary file loading
void SDRAM::load(const char *name, vluint32_t size, vluint32_t addr)
{
    FILE *fh;
    
    fh = fopen(name, "rb");
    if (fh)
    {
        int        row_size; // Row size (num_cols * 1, 2 or 4)
        vluint8_t *row_buf;  // Row buffer
        int        row_pos;  // Row position (0 to num_rows - 1)
        int        bank_nr;  // Bank number (0 to 3)
        int        idx;      // Array index (0 to num_cols * num_rows - 1)
        
        // Row size computation based on data bus width
        row_size = (int)1 << (bit_cols + bus_log2);
        // Allocate one full row
        row_buf = new vluint8_t[row_size];
        
        // Row position
        row_pos = (int)addr >> (bit_cols + bus_log2);
        // Banks layout
        if (mem_flags & FLAG_BANK_INTERLEAVING)
        {
            // Banks are interleaved
            bank_nr = row_pos & (SDRAM_NUM_BANKS - 1);
            row_pos = row_pos >> SDRAM_BIT_BANKS;
        }
        else
        {
            // Banks are contiguous
            bank_nr = row_pos >> bit_rows;
            row_pos = row_pos & (num_rows - 1);
        }
        idx = row_pos << (bit_cols + bus_log2);
        
        printf("Starting row : %d, starting bank : %d\n", row_pos, bank_nr);
        printf("Loading 0x%08lX bytes @ 0x%08lX from binary file \"%s\"...", size, addr, name);
        for (int r = 0; r < (int)size; r += row_size) // row
        {
            // Read one full row from the binary file
            fread((void *)row_buf, row_size, 1, fh);
            
            // Here, we take care of the endianness
            if (mem_flags & FLAG_BIG_ENDIAN)
            {
                // MSB first (motorola's way)
                for (int c = 0; c < row_size; c++) // column
                {
                #if BYTE_ORDER == LITTLE_ENDIAN
                    array_u8[bank_nr][idx ^ bus_mask] = row_buf[c];
                #else
                    array_u8[bank_nr][idx] = row_buf[c];
                #endif
                    // Next byte
                    idx++;
                }
            }
            else
            {
                // LSB first (intel's way)
                for (int c = 0; c < row_size; c++) // column
                {
                #if BYTE_ORDER == BIG_ENDIAN
                    array_u8[bank_nr][idx ^ bus_mask] = row_buf[c];
                #else
                    array_u8[bank_nr][idx] = row_buf[c];
                #endif
                    // Next byte
                    idx++;
                }
            }
            
            // Compute next row's address
            if (mem_flags & FLAG_BANK_INTERLEAVING)
            {
                // Increment bank number
                bank_nr = (bank_nr + 1) & (SDRAM_NUM_BANKS - 1);
                
                // Bank #3 -> bank #0
                if (!bank_nr)
                {
                    row_pos ++;
                    if ((row_pos == (int)num_rows) && ((r + row_size) < (int)size))
                    {
                        printf("Memory overflow while loading !!\n");
                        return;
                    }
                }
                else
                {
                    idx -= (int)num_cols;
                }
            }
            else
            {
                // Increment row position
                row_pos = (row_pos + 1) & ((int)num_rows - 1);
                
                // Last row in a bank
                if (!row_pos)
                {
                    idx = 0;
                    bank_nr++;
                    if ((bank_nr == SDRAM_NUM_BANKS) && ((r + row_size) < (int)size))
                    {
                        printf("Memory overflow while loading !!\n");
                        return;
                    }
                }
            }
        }
        printf("OK\n");
        
        delete[] row_buf;
    }
    else
    {
        printf("Cannot load binary file \"%s\" !!\n", name);
    }
}

// Binary file saving
void SDRAM::save(const char *name, vluint32_t size, vluint32_t addr)
{
    FILE *fh;
    
    fh = fopen(name, "wb");
    if (fh)
    {
        int        row_size; // Row size (num_cols * 1, 2 or 4)
        vluint8_t *row_buf;  // Row buffer
        int        row_pos;  // Row position (0 to num_rows - 1)
        int        bank_nr;  // Bank number (0 to 3)
        int        idx;      // Array index (0 to num_cols * num_rows - 1)
        
        // Row size computation based on data bus width
        row_size = (int)1 << (bit_cols + bus_log2);
        // Allocate one full row
        row_buf = new vluint8_t[row_size];
        
        // Row position
        row_pos = (int)addr >> (bit_cols + bus_log2);
        // Banks layout
        if (mem_flags & FLAG_BANK_INTERLEAVING)
        {
            // Banks are interleaved
            bank_nr = row_pos & (SDRAM_NUM_BANKS - 1);
            row_pos = row_pos >> SDRAM_BIT_BANKS;
        }
        else
        {
            // Banks are contiguous
            bank_nr = row_pos >> bit_rows;
            row_pos = row_pos & (num_rows - 1);
        }
        idx = row_pos << (bit_cols + bus_log2);
        
        printf("Saving 0x%08lX bytes @ 0x%08lX to binary file \"%s\"...", size, addr, name);
        for (int r = 0; r < (int)size; r += row_size) // row
        {
            // Here, we take care of the endianness
            if (mem_flags & FLAG_BIG_ENDIAN)
            {
                // MSB first (motorola's way)
                for (int c = 0; c < row_size; c++) // column
                {
                #if BYTE_ORDER == LITTLE_ENDIAN
                    row_buf[c] = array_u8[bank_nr][idx ^ bus_mask];
                #else
                    row_buf[c] = array_u8[bank_nr][idx];
                #endif
                    // Next byte
                    idx++;
                }
            }
            else
            {
                // LSB first (intel's way)
                for (int c = 0; c < row_size; c++) // column
                {
                #if BYTE_ORDER == BIG_ENDIAN
                    row_buf[c] = array_u8[bank_nr][idx ^ bus_mask];
                #else
                    row_buf[c] = array_u8[bank_nr][idx];
                #endif
                    // Next byte
                    idx++;
                }
            }
            
            // Compute next row's address
            if (mem_flags & FLAG_BANK_INTERLEAVING)
            {
                // Increment bank number
                bank_nr = (bank_nr + 1) & (SDRAM_NUM_BANKS - 1);
                
                // Bank #3 -> bank #0
                if (!bank_nr)
                {
                    row_pos ++;
                    if ((row_pos == (int)num_rows) && ((r + row_size) < (int)size))
                    {
                        printf("Memory overflow while saving !!\n");
                        return;
                    }
                }
                else
                {
                    idx -= (int)num_cols;
                }
            }
            else
            {
                // Increment row position
                row_pos = (row_pos + 1) & ((int)num_rows - 1);
                
                // Last row in a bank
                if (!row_pos)
                {
                    idx = 0;
                    bank_nr++;
                    if ((bank_nr == SDRAM_NUM_BANKS) && ((r + row_size) < (int)size))
                    {
                        printf("Memory overflow while saving !!\n");
                        return;
                    }
                }
            }
            
            // Write one full row to the binary file
            fwrite((void *)row_buf, row_size, 1, fh);
        }
        printf("OK\n");
        
        delete[] row_buf;
    }
    else
    {
        printf("Cannot save binary file \"%s\" !!\n", name);
    }
}

// Read a byte
vluint8_t SDRAM::read_byte(vluint32_t addr)
{
    return (this->*read_u8_priv)(addr);
}

// Read a word
vluint16_t SDRAM::read_word(vluint32_t addr)
{
    return (this->*read_u16_priv)(addr);
}

// Read a long
vluint32_t SDRAM::read_long(vluint32_t addr)
{
    return (this->*read_u32_priv)(addr);
}

// Read a quad
vluint64_t SDRAM::read_quad(vluint32_t addr)
{
    return (this->*read_u64_priv)(addr);
}

// Write a byte
void SDRAM::write_byte(vluint32_t addr, vluint8_t data)
{
    return (this->*write_u8_priv)(addr, data);
}

// Write a word
void SDRAM::write_word(vluint32_t addr, vluint16_t data)
{
    return (this->*write_u16_priv)(addr, data);
}

// Write a long
void SDRAM::write_long(vluint32_t addr, vluint32_t data)
{
    return (this->*write_u32_priv)(addr, data);
}

// Write a quad
void SDRAM::write_quad(vluint32_t addr, vluint64_t data)
{
    return (this->*write_u64_priv)(addr, data);
}

// Cycle evaluate
void SDRAM::eval
(
    // Timestamp from clock generator
    vluint64_t ts,
    // SDRAM clock
    vluint8_t clk,
    vluint8_t cke,
    // SDRAM commands
    vluint8_t cs_n,
    vluint8_t ras_n,
    vluint8_t cas_n,
    vluint8_t we_n,
    // SDRAM address
    vluint8_t ba,
    vluint16_t addr,
    // SDRAM data bus
    vluint8_t dqm,
    vluint64_t dq_in,
    vluint64_t &dq_out
)
{
    // Clock enabled
    if (cke)
    {
        // Rising edge on clock
        if (clk && !(prev_clk))
        {
            // Command pipeline
            cmd_pipe.pipe >>= 8;
            cmd_pipe.u8[3] = CMD_NOP;
            col_pipe.pipe >>= 16;
            ba_pipe.pipe  >>= 8;
            bap_pipe.pipe >>= 8;
            a10_pipe.pipe >>= 16;

            // DQM pipeline
            dqm_pipe[0] = dqm_pipe[1];
            dqm_pipe[1] = dqm;
            
            // Process SDRAM command (immediate)
            if (!cs_n)
            {
                // cmd[2:0] = { ras_n, cas_n, we_n }
                vluint8_t  cmd = (ras_n << 2) | (cas_n << 1) | we_n;
                // addr[10] wire
                vluint16_t a10 = addr & 0x400;
                
                switch (cmd)
                {
                    // 3'b000 : Load mode register
                    case CMD_LMR:
                    {
                        if (dbg_on)
                        {
                            printf("Load Std Mode Register @ %llu ps\n", ts);
                            log_size += sprintf(log_buf + log_size, "%15llu ps : Load Std Mode Register\n", ts);
                        }
                            
                        // CAS latency
                        switch((addr >> 4) & 7)
                        {
                            case 2:
                            {
                                if (dbg_on)
                                {
                                    printf("CAS latency        = 2 cycles\n");
                                    log_size += sprintf(log_buf + log_size, "                     CAS latency        = 2 cycles\n");
                                }
                                cas_lat = (int)2;
                                break;
                            }
                            case 3:
                            {
                                if (dbg_on)
                                {
                                    printf("CAS latency        = 3 cycles\n");
                                    log_size += sprintf(log_buf + log_size, "                     CAS latency        = 3 cycles\n");
                                }
                                cas_lat = (int)3;
                                break;
                            }
                            default:
                            {
                                if (dbg_on)
                                {
                                    printf("CAS latency        = ???\n");
                                    log_size += sprintf(log_buf + log_size, "                     CAS latency        = ???\n");
                                }
                                cas_lat = (int)0; // This disables pipelined commands
                            }
                        }
                        
                        // Burst length + Burst type
                        bst_type = (vluint8_t)0;
                        switch (addr & 0xF)
                        {
                            case 0x8:
                            case 0x0:
                            {
                                if (dbg_on)
                                {
                                    printf("Read burst length  = 1 word\n");
                                    log_size += sprintf(log_buf + log_size, "                     Read burst length  = 1 word\n");
                                }
                                bst_len_rd = 1;
                                bst_msk_rd = 0;
                                break;
                            }
                            case 0x9:
                            case 0x1:
                            {
                                if (dbg_on)
                                {
                                    printf("Read burst length  = 2 words\n");
                                    log_size += sprintf(log_buf + log_size, "                     Read burst length  = 2 words\n");
                                }
                                bst_len_rd = 2;
                                bst_msk_rd = 1;
                                break;
                            }
                            case 0xA:
                            {
                                bst_type = (vluint8_t)1;
                            }
                            case 0x2:
                            {
                                if (dbg_on)
                                {
                                    printf("Read burst length  = 4 words\n");
                                    log_size += sprintf(log_buf + log_size, "                     Read burst length  = 4 words\n");
                                }
                                bst_len_rd = 4;
                                bst_msk_rd = 3;
                                break;
                            }
                            case 0xB:
                            {
                                bst_type = (vluint8_t)1;
                            }
                            case 0x3:
                            {
                                if (dbg_on)
                                {
                                    printf("Read burst length  = 8 words\n");
                                    log_size += sprintf(log_buf + log_size, "                     Read burst length  = 8 words\n");
                                }
                                bst_len_rd = 8;
                                bst_msk_rd = 7;
                                break;
                            }
                            case 0x7:
                            {
                                if (dbg_on)
                                {
                                    printf("Read burst length  = continuous\n");
                                    log_size += sprintf(log_buf + log_size, "                     Read burst length  = continuous\n");
                                }
                                bst_len_rd = num_cols;
                                bst_msk_rd = num_cols - 1;
                                break;
                            }
                            default:
                            {
                                if (dbg_on)
                                {
                                    printf("Read burst length  = ???\n");
                                    log_size += sprintf(log_buf + log_size, "                     Read burst length  = ???\n");
                                }
                                bst_len_rd = 0; // This will disable burst read
                                bst_msk_rd = 0;
                            }
                        }
                        if (dbg_on)
                        {
                            if (addr & 8)
                            {
                                printf("Burst type         = interleaved\n");
                                log_size += sprintf(log_buf + log_size, "                     Burst type         = interleaved\n");
                            }
                            else
                            {
                                printf("Burst type         = sequential\n");
                                log_size += sprintf(log_buf + log_size, "                     Burst type         = sequential\n");
                            }
                        }
                        
                        // Write burst
                        if (addr & 0x0200)
                        {
                            if (dbg_on)
                            {
                                printf("Write burst length = 1\n");
                                log_size += sprintf(log_buf + log_size, "                     Write burst length = 1\n");
                            }
                            bst_len_wr = 1;
                            bst_msk_wr = 0;
                        }
                        else
                        {
                            if (dbg_on)
                            {
                                if (bst_len_rd)
                                {
                                    if (bst_len_rd <= (int)8)
                                    {
                                        printf("Write burst length = %d word(s)\n", bst_len_rd);
                                        log_size += sprintf(log_buf + log_size, "                     Write burst length = %d word(s)\n", bst_len_rd);
                                    }
                                    else
                                    {
                                        printf("Write burst length = continuous\n");
                                        log_size += sprintf(log_buf + log_size, "                     Write burst length = continuous\n");
                                    }
                                }
                                else
                                {
                                    // This disables burst write
                                    printf("Write burst length = ???\n");
                                    log_size += sprintf(log_buf + log_size, "                     Write burst length = ???\n");
                                }
                            }
                            bst_len_wr = bst_len_rd;
                            bst_msk_wr = bst_msk_rd;
                        }
                        break;
                    }
                    // 3'b001 : Auto refresh
                    case CMD_REF:
                    {
                        if (dbg_on)
                            log_size += sprintf(log_buf + log_size, "%15llu ps : Auto Refresh\n", ts);
                        
                        for (int i = 0; i < SDRAM_NUM_BANKS; i++)
                        {
                            if (!row_pre[i])
                            {
                                printf("ERROR @ %llu ps : All banks must be Precharge before Auto Refresh\n", ts);
                                break;
                            }
                        }
                        break;
                    }
                    // 3'b010 : Precharge
                    case CMD_PRE:
                    {
                        if (a10)
                        {
                            if (dbg_on)
                                log_size += sprintf(log_buf + log_size, "%15llu ps : Precharge all banks\n", ts);
                            
                            if (ap_bank[0] || ap_bank[1] || ap_bank[2] || ap_bank[3])
                            {
                                printf("ERROR @ %llu ps : at least one bank is auto-precharged !\n", ts);
                                break;
                            }
                            
                            // Precharge all banks
                            for (int i = 0; i < SDRAM_NUM_BANKS; i++)
                            {
                                row_act[i] = 0;
                                row_pre[i] = 1;
                            }
                        }
                        else
                        {
                            if (dbg_on)
                                log_size += sprintf(log_buf + log_size, "%15llu ps : Precharge bank #%d\n", ts, ba);
                                
                            if (ap_bank[ba])
                            {
                                printf("ERROR @ %llu ps : cannot apply a precharge to auto-precharged bank %d !\n", ts, ba);
                                break;
                            }
                            
                            // Precharge one bank
                            row_act[ba] = 0;
                            row_pre[ba] = 1;
                        }
                        
                        // Terminate a WRITE immediately
                        if ((a10) || (bank == (int)ba))
                            bst_ctr_wr = 0;
                        
                        // CAS latency pipeline for READ
                        if (cas_lat)
                        {
                            cmd_pipe.u8[cas_lat] = CMD_PRE;
                            bap_pipe.u8[cas_lat] = ba;
                            a10_pipe.u16[cas_lat] = a10;
                        }
                        
                        break;
                    }
                    // 3'b011 : Activate
                    case CMD_ACT:
                    {
                        if (dbg_on)
                            log_size += sprintf(log_buf + log_size, "%15llu ps : Activate bank #%d, row #%d\n", ts, ba, addr);
                                
                        if (row_act[ba])
                        {
                            printf("ERROR @ %llu ps : bank %d already active !\n", ts, ba);
                            break;
                        }
                           
                        row_act[ba]  = 1;
                        row_pre[ba]  = 0;
                        row_addr[ba] = ((int)addr << bit_cols) & mask_rows;
                        
                        break;
                    }
                    // 3'b100 : Write
                    case CMD_WR:
                    {
                        if (dbg_on)
                            log_size += sprintf(log_buf + log_size,
                                                "%15llu ps : Write%s bank #%d, col #%d\n",
                                                ts, (a10) ? "(AP)" : "", ba, addr & mask_cols);
                        
                        if (!row_act[ba])
                        {
                            printf("ERROR @ %llu ps : bank %d is not activated for WRITE !\n", ts, ba);
                            break;
                        }
                           
                        // Latch command right away
                        cmd_pipe.u8[0]  = CMD_WR;
                        col_pipe.u16[0] = addr & mask_cols;
                        ba_pipe.u8[0]   = ba;
                
                        // Auto-precharge
                        ap_bank[ba] = a10;
                        
                        break;
                    }
                    // 3'b101 : Read
                    case CMD_RD:
                    {
                        if (dbg_on)
                            log_size += sprintf(log_buf + log_size,
                                                "%15llu ps : Read%s bank #%d, col #%d\n",
                                                ts, (a10) ? "(AP)" : "", ba, addr & mask_cols);
                        
                        if (!row_act[ba])
                        {
                            printf("ERROR @ %llu ps : bank %d is not activated for READ !\n", ts, ba);
                            break;
                        }
                           
                        // CAS latency pipeline
                        if (cas_lat)
                        {
                            cmd_pipe.u8[cas_lat]  = CMD_RD;
                            col_pipe.u16[cas_lat] = addr & mask_cols;
                            ba_pipe.u8[cas_lat]   = ba;
                        }
                        
                        // Auto-precharge
                        ap_bank[ba] = a10;
                        
                        break;
                    }
                    // 3'b110 : Burst stop
                    case CMD_BST:
                    {
                        if (dbg_on)
                            log_size += sprintf(log_buf + log_size, "%15llu ps : Burst Stop bank #%d\n", ts, ba);
                            
                        if (ap_bank[ba])
                        {
                            printf("ERROR @ %llu ps : cannot apply a burst stop to auto-precharged bank %d !\n", ts, ba);
                            break;
                        }
                            
                        // Terminate a WRITE immediately
                        bst_ctr_wr = (vluint16_t)0;
                        
                        // CAS latency for READ
                        if (cas_lat)
                        {
                            cmd_pipe.u8[cas_lat] = CMD_BST;
                        }
                        break;
                    }
                    // 3'b111 : No operation
                    default: ;
                }
            }
            
            // Process SDRAM command (pipelined)
            switch (cmd_pipe.u8[0])
            {
                // 3'b010 : Precharge
                case CMD_PRE:
                {
                    if ((a10_pipe.u16[0]) || (bap_pipe.u8[0] == (vluint8_t)bank))
                        bst_ctr_rd = (int)0;
                    break;
                }
                // 3'b100 : Write
                case CMD_WR:
                {
                    // Bank, row and column addresses in memory array
                    bank       = (int)ba_pipe.u8[0];
                    row        = row_addr[bank] + ((int)col_pipe.u16[0] & ~bst_msk_wr);
                    col        = (int)col_pipe.u16[0] & bst_msk_wr;
                    bst_ctr_rd = 0;
                    bst_ctr_wr = bst_len_wr;
                    
                    if (dbg_on)
                    {
                        if (mem_flags & FLAG_BANK_INTERLEAVING)
                            fprintf(fh_log, "   Wr @ 0x%08X :", ((row_addr[bank] << SDRAM_BIT_BANKS) + (bank << bit_cols) + (int)col_pipe.u16[0]) << bus_log2);
                        else
                            fprintf(fh_log, "   Wr @ 0x%08X :", (row_addr[bank] + (bank << (bit_rows + bit_cols)) + (int)col_pipe.u16[0]) << bus_log2);
                    }
                    
                    break;
                }
                // 3'b101 : Read
                case CMD_RD:
                {
                    // Bank, row and column addresses in memory array
                    bank       = (int)ba_pipe.u8[0];
                    row        = row_addr[bank] + ((int)col_pipe.u16[0] & ~bst_msk_rd);
                    col        = (int)col_pipe.u16[0] & bst_msk_rd;
                    bst_ctr_rd = bst_len_rd;
                    bst_ctr_wr = 0;
                    
                    if (dbg_on)
                    {
                        if (mem_flags & FLAG_BANK_INTERLEAVING)
                            fprintf(fh_log, "   Rd @ 0x%08X :", ((row_addr[bank] << SDRAM_BIT_BANKS) + (bank << bit_cols) + (int)col_pipe.u16[0]) << bus_log2);
                        else
                            fprintf(fh_log, "   Rd @ 0x%08X :", (row_addr[bank] + (bank << (bit_rows + bit_cols)) + (int)col_pipe.u16[0]) << bus_log2);
                    }
                    
                    break;
                }
                // 3'b110 : Burst stop
                case CMD_BST:
                {
                    bst_ctr_rd = (int)0;
                    break;
                }
                // 3'b111 : No operation
                default: ;
            }
            
            // Write to memory
            if (bst_ctr_wr)
            {
                switch (bus_log2)
                {
                    // 8-bit bus
                    case 0:
                    {
                        // Non masked write
                        if (!dqm)
                        {
                            array_u8[bank][row + col] = (vluint8_t)dq_in;
                        }
                        break;
                    }
                    // 16-bit bus
                    case 1:
                    {
                        // Non masked write
                        if (!dqm)
                        {
                            array_u16[bank][row + col] = (vluint16_t)dq_in;
                        }
                        // Masked write
                        else
                        {
                            vluint16_t dqm_mask = (vluint16_t)c_dqm_mask[dqm];
                            
                            array_u16[bank][row + col] = (vluint16_t)dq_in & dqm_mask
                                                       | array_u16[bank][row + col] & ~dqm_mask;
                        }
                        break;
                    }
                    // 32-bit bus
                    case 2:
                    {
                        // Non masked write
                        if (!dqm)
                        {
                            array_u32[bank][row + col] = (vluint32_t)dq_in;
                        }
                        // Masked write
                        else
                        {
                            vluint32_t dqm_mask = (vluint32_t)c_dqm_mask[dqm];
                            
                            array_u32[bank][row + col] = (vluint32_t)dq_in & dqm_mask
                                                       | array_u32[bank][row + col] & ~dqm_mask;
                        }
                        break;
                    }
                    // 64-bit bus
                    case 3:
                    {
                        // Non masked write
                        if (!dqm)
                        {
                            array_u64[bank][row + col] = dq_in;
                        }
                        // Masked write
                        else
                        {
                            vluint64_t dqm_mask = c_dqm_mask[dqm];
                            
                            array_u64[bank][row + col] = (dq_in & dqm_mask)
                                                       | array_u64[bank][row + col] & ~dqm_mask;
                        }
                        break;
                    }
                }
                
                if (dbg_on)
                {
                    dq_lane_t dq_tmp;
                    dq_tmp.u64 = dq_in;
                    
                    fputc(' ', fh_log);
                    for (int l = bus_mask; l >= 0; l--)
                    {
                        if ((dqm >> l) & 1)
                        {
                            fputs("XX", fh_log);
                        }
                        else
                        {
                            fprintf(fh_log, "%02X", dq_tmp.u8[l]);
                        }
                    }
                }
                
                // Burst counter
                if (--bst_ctr_wr)
                {
                    if (bst_type)
                    {
                        // BL4_Int, BL8_Int
                        col = c_col_int[col][bst_ctr_wr];
                    }
                    else
                    {
                        // BL1, BL2, BL4_Seq, BL8_Seq
                        col++;
                        col &= bst_msk_wr;
                    }
                }
                // End of burst
                else
                {
                    // Auto-precharge case
                    if (ap_bank[bank])
                    {
                        ap_bank[bank] = (vluint16_t)0;
                        row_act[bank] = (vluint8_t)0;
                        row_pre[bank] = (vluint8_t)1;
                    }
                    if (dbg_on) fputs("\n", fh_log);
                }
            }
            // Read from memory
            else if (bst_ctr_rd)
            {
                switch (bus_log2)
                {
                    // 8-bit bus
                    case 0:
                    {
                        dq_out = (vluint64_t)array_u8[bank][row + col] & c_dqm_mask[dqm];
                        break;
                    }
                    // 16-bit bus
                    case 1:
                    {
                        dq_out = (vluint64_t)array_u16[bank][row + col] & c_dqm_mask[dqm];
                        break;
                    }
                    // 32-bit bus
                    case 2:
                    {
                        dq_out = (vluint64_t)array_u32[bank][row + col] & c_dqm_mask[dqm];
                        break;
                    }
                    // 64-bit bus
                    case 3:
                    {
                        dq_out = (vluint64_t)array_u64[bank][row + col] & c_dqm_mask[dqm];
                        break;
                    }
                    default:
                    {
                        dq_out = (vluint64_t)0;
                    }
                }
                
                if (dbg_on)
                {
                    dq_lane_t dq_tmp;
                    dq_tmp.u64 = dq_out;
                    
                    fputc(' ', fh_log);
                    for (int l = bus_mask; l >= 0; l--)
                    {
                        if ((dqm_pipe[0] >> l) & 1)
                        {
                            fputs("XX", fh_log);
                        }
                        else
                        {
                            fprintf(fh_log, "%02X", dq_tmp.u8[l]);
                        }
                    }
                }
                
                // Burst counter
                if (--bst_ctr_rd)
                {
                    if (bst_type)
                    {
                        // BL4_Int, BL8_Int
                        col = c_col_int[col][bst_ctr_rd];
                    }
                    else
                    {
                        // BL1, BL2, BL4_Seq, BL8_Seq
                        col++;
                        col &= bst_msk_rd;
                    }
                }
                // End of burst
                else
                {
                    // Auto-precharge case
                    if (ap_bank[bank])
                    {
                        ap_bank[bank] = (vluint16_t)0;
                        row_act[bank] = (vluint8_t)0;
                        row_pre[bank] = (vluint8_t)1;
                    }
                    if (dbg_on) fputs("\n", fh_log);
                }
            }
            // Flush log buffer
            if (log_size)
            {
                fprintf(fh_log, log_buf);
                fflush(fh_log);
                log_size = 0;
            }
        }
        
        // For edge detection
        prev_clk = clk;
    }
    // Clock disabled
    else
    {
        prev_clk = (vluint8_t)0;
    }
}

// Read a byte, interleaved banks
vluint8_t SDRAM::read_u8_i(vluint32_t addr)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |      rows      |  banks  |    columns     |
    // |<-- bit_rows -->|<-- 2 -->|<-- bit_cols -->|
    bank_nr = (addr & mask_bank) >> bit_cols;
    idx     = (addr & mask_cols) | ((addr & mask_rows) >> SDRAM_BIT_BANKS);

    return array_u8[bank_nr][idx];
}

// Read a word, interleaved banks, big-endian
vluint16_t SDRAM::read_u16_i_be(vluint32_t addr)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |      rows      |  banks  |    columns     |  data   |
    // |<-- bit_rows -->|<-- 2 -->|<-- bit_cols -->|<-- 1 -->|
    addr  >>= 1;
    bank_nr = (addr & mask_bank) >> bit_cols;
    idx     = (addr & mask_cols) | ((addr & mask_rows) >> SDRAM_BIT_BANKS);

#if BYTE_ORDER == LITTLE_ENDIAN
    return __builtin_bswap16(array_u16[bank_nr][idx]);
#else
    return array_u16[bank_nr][idx];
#endif
}

// Read a word, interleaved banks, little-endian
vluint16_t SDRAM::read_u16_i_le(vluint32_t addr)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |      rows      |  banks  |    columns     |  data   |
    // |<-- bit_rows -->|<-- 2 -->|<-- bit_cols -->|<-- 1 -->|
    addr  >>= 1;
    bank_nr = (addr & mask_bank) >> bit_cols;
    idx     = (addr & mask_cols) | ((addr & mask_rows) >> SDRAM_BIT_BANKS);

#if BYTE_ORDER == BIG_ENDIAN
    return __builtin_bswap16(array_u16[bank_nr][idx]);
#else
    return array_u16[bank_nr][idx];
#endif
}

// Read a long, interleaved banks, big-endian
vluint32_t SDRAM::read_u32_i_be(vluint32_t addr)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |      rows      |  banks  |    columns     |  data   |
    // |<-- bit_rows -->|<-- 2 -->|<-- bit_cols -->|<-- 2 -->|
    addr  >>= 2;
    bank_nr = (addr & mask_bank) >> bit_cols;
    idx     = (addr & mask_cols) | ((addr & mask_rows) >> SDRAM_BIT_BANKS);

#if BYTE_ORDER == LITTLE_ENDIAN
    return __builtin_bswap32(array_u32[bank_nr][idx]);
#else
    return array_u32[bank_nr][idx];
#endif
}

// Read a long, interleaved banks, little-endian
vluint32_t SDRAM::read_u32_i_le(vluint32_t addr)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |      rows      |  banks  |    columns     |  data   |
    // |<-- bit_rows -->|<-- 2 -->|<-- bit_cols -->|<-- 2 -->|
    addr  >>= 2;
    bank_nr = (addr & mask_bank) >> bit_cols;
    idx     = (addr & mask_cols) | ((addr & mask_rows) >> SDRAM_BIT_BANKS);

#if BYTE_ORDER == BIG_ENDIAN
    return __builtin_bswap32(array_u32[bank_nr][idx]);
#else
    return array_u32[bank_nr][idx];
#endif
}

// Read a quad, interleaved banks, big-endian
vluint64_t SDRAM::read_u64_i_be(vluint32_t addr)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |      rows      |  banks  |    columns     |  data   |
    // |<-- bit_rows -->|<-- 2 -->|<-- bit_cols -->|<-- 3 -->|
    addr  >>= 3;
    bank_nr = (addr & mask_bank) >> bit_cols;
    idx     = (addr & mask_cols) | ((addr & mask_rows) >> SDRAM_BIT_BANKS);

#if BYTE_ORDER == LITTLE_ENDIAN
    return __builtin_bswap64(array_u64[bank_nr][idx]);
#else
    return array_u64[bank_nr][idx];
#endif
}

// Read a quad, interleaved banks, little-endian
vluint64_t SDRAM::read_u64_i_le(vluint32_t addr)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |      rows      |  banks  |    columns     |  data   |
    // |<-- bit_rows -->|<-- 2 -->|<-- bit_cols -->|<-- 3 -->|
    addr  >>= 3;
    bank_nr = (addr & mask_bank) >> bit_cols;
    idx     = (addr & mask_cols) | ((addr & mask_rows) >> SDRAM_BIT_BANKS);

#if BYTE_ORDER == BIG_ENDIAN
    return __builtin_bswap64(array_u64[bank_nr][idx]);
#else
    return array_u64[bank_nr][idx];
#endif
}

// Read a byte, contiguous banks
vluint8_t SDRAM::read_u8_c(vluint32_t addr)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |  banks  |      rows      |    columns     |
    // |<-- 2 -->|<-- bit_rows -->|<-- bit_cols -->|
    bank_nr = (addr & mask_bank) >> (bit_cols + bit_rows);
    idx     = (addr & (mask_cols | mask_rows));

    return array_u8[bank_nr][idx];
}

// Read a word, contiguous banks, big-endian
vluint16_t SDRAM::read_u16_c_be(vluint32_t addr)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |  banks  |      rows      |    columns     |  data   |
    // |<-- 2 -->|<-- bit_rows -->|<-- bit_cols -->|<-- 1 -->|
    addr  >>= 1;
    bank_nr = (addr & mask_bank) >> (bit_cols + bit_rows);
    idx     = (addr & (mask_cols | mask_rows));

#if BYTE_ORDER == LITTLE_ENDIAN
    return __builtin_bswap16(array_u16[bank_nr][idx]);
#else
    return array_u16[bank_nr][idx];
#endif
}

// Read a word, contiguous banks, little-endian
vluint16_t SDRAM::read_u16_c_le(vluint32_t addr)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |  banks  |      rows      |    columns     |  data   |
    // |<-- 2 -->|<-- bit_rows -->|<-- bit_cols -->|<-- 1 -->|
    addr  >>= 1;
    bank_nr = (addr & mask_bank) >> (bit_cols + bit_rows);
    idx     = (addr & (mask_cols | mask_rows));

#if BYTE_ORDER == BIG_ENDIAN
    return __builtin_bswap16(array_u16[bank_nr][idx]);
#else
    return array_u16[bank_nr][idx];
#endif
}

// Read a long, contiguous banks, big-endian
vluint32_t SDRAM::read_u32_c_be(vluint32_t addr)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |  banks  |      rows      |    columns     |  data   |
    // |<-- 2 -->|<-- bit_rows -->|<-- bit_cols -->|<-- 2 -->|
    addr  >>= 2;
    bank_nr = (addr & mask_bank) >> (bit_cols + bit_rows);
    idx     = (addr & (mask_cols | mask_rows));

#if BYTE_ORDER == LITTLE_ENDIAN
    return __builtin_bswap32(array_u32[bank_nr][idx]);
#else
    return array_u32[bank_nr][idx];
#endif
}

// Read a long, contiguous banks, little-endian
vluint32_t SDRAM::read_u32_c_le(vluint32_t addr)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |  banks  |      rows      |    columns     |  data   |
    // |<-- 2 -->|<-- bit_rows -->|<-- bit_cols -->|<-- 2 -->|
    addr  >>= 2;
    bank_nr = (addr & mask_bank) >> (bit_cols + bit_rows);
    idx     = (addr & (mask_cols | mask_rows));

#if BYTE_ORDER == BIG_ENDIAN
    return __builtin_bswap32(array_u32[bank_nr][idx]);
#else
    return array_u32[bank_nr][idx];
#endif
}

// Read a quad, contiguous banks, big-endian
vluint64_t SDRAM::read_u64_c_be(vluint32_t addr)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |  banks  |      rows      |    columns     |  data   |
    // |<-- 2 -->|<-- bit_rows -->|<-- bit_cols -->|<-- 3 -->|
    addr  >>= 3;
    bank_nr = (addr & mask_bank) >> (bit_cols + bit_rows);
    idx     = (addr & (mask_cols | mask_rows));

#if BYTE_ORDER == LITTLE_ENDIAN
    return __builtin_bswap64(array_u64[bank_nr][idx]);
#else
    return array_u64[bank_nr][idx];
#endif
}

// Read a quad, contiguous banks, little-endian
vluint64_t SDRAM::read_u64_c_le(vluint32_t addr)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |  banks  |      rows      |    columns     |  data   |
    // |<-- 2 -->|<-- bit_rows -->|<-- bit_cols -->|<-- 3 -->|
    addr  >>= 3;
    bank_nr = (addr & mask_bank) >> (bit_cols + bit_rows);
    idx     = (addr & (mask_cols | mask_rows));

#if BYTE_ORDER == BIG_ENDIAN
    return __builtin_bswap64(array_u64[bank_nr][idx]);
#else
    return array_u64[bank_nr][idx];
#endif
}

// Write a byte, interleaved banks
void SDRAM::write_u8_i(vluint32_t addr, vluint8_t data)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |      rows      |  banks  |    columns     |
    // |<-- bit_rows -->|<-- 2 -->|<-- bit_cols -->|
    bank_nr = (addr & mask_bank) >> bit_cols;
    idx     = (addr & mask_cols) | ((addr & mask_rows) >> SDRAM_BIT_BANKS);

    array_u8[bank_nr][idx] = data;
}

// Write a word, interleaved banks, big-endian
void SDRAM::write_u16_i_be(vluint32_t addr, vluint16_t data)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |      rows      |  banks  |    columns     |  data   |
    // |<-- bit_rows -->|<-- 2 -->|<-- bit_cols -->|<-- 1 -->|
    addr  >>= 1;
    bank_nr = (addr & mask_bank) >> bit_cols;
    idx     = (addr & mask_cols) | ((addr & mask_rows) >> SDRAM_BIT_BANKS);

#if BYTE_ORDER == LITTLE_ENDIAN
    array_u16[bank_nr][idx] = __builtin_bswap16(data);
#else
    array_u16[bank_nr][idx] = data;
#endif
}

// Write a word, interleaved banks, little-endian
void SDRAM::write_u16_i_le(vluint32_t addr, vluint16_t data)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |      rows      |  banks  |    columns     |  data   |
    // |<-- bit_rows -->|<-- 2 -->|<-- bit_cols -->|<-- 1 -->|
    addr  >>= 1;
    bank_nr = (addr & mask_bank) >> bit_cols;
    idx     = (addr & mask_cols) | ((addr & mask_rows) >> SDRAM_BIT_BANKS);

#if BYTE_ORDER == BIG_ENDIAN
    array_u16[bank_nr][idx] = __builtin_bswap16(data);
#else
    array_u16[bank_nr][idx] = data;
#endif
}

// Write a long, interleaved banks, big-endian
void SDRAM::write_u32_i_be(vluint32_t addr, vluint32_t data)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |      rows      |  banks  |    columns     |  data   |
    // |<-- bit_rows -->|<-- 2 -->|<-- bit_cols -->|<-- 2 -->|
    addr  >>= 2;
    bank_nr = (addr & mask_bank) >> bit_cols;
    idx     = (addr & mask_cols) | ((addr & mask_rows) >> SDRAM_BIT_BANKS);

#if BYTE_ORDER == LITTLE_ENDIAN
    array_u32[bank_nr][idx] = __builtin_bswap32(data);
#else
    array_u32[bank_nr][idx] = data;
#endif
}

// Write a long, interleaved banks, little-endian
void SDRAM::write_u32_i_le(vluint32_t addr, vluint32_t data)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |      rows      |  banks  |    columns     |  data   |
    // |<-- bit_rows -->|<-- 2 -->|<-- bit_cols -->|<-- 2 -->|
    addr  >>= 2;
    bank_nr = (addr & mask_bank) >> bit_cols;
    idx     = (addr & mask_cols) | ((addr & mask_rows) >> SDRAM_BIT_BANKS);

#if BYTE_ORDER == BIG_ENDIAN
    array_u32[bank_nr][idx] = __builtin_bswap32(data);
#else
    array_u32[bank_nr][idx] = data;
#endif
}

// Write a quad, interleaved banks, big-endian
void SDRAM::write_u64_i_be(vluint32_t addr, vluint64_t data)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |      rows      |  banks  |    columns     |  data   |
    // |<-- bit_rows -->|<-- 2 -->|<-- bit_cols -->|<-- 3 -->|
    addr  >>= 3;
    bank_nr = (addr & mask_bank) >> bit_cols;
    idx     = (addr & mask_cols) | ((addr & mask_rows) >> SDRAM_BIT_BANKS);

#if BYTE_ORDER == LITTLE_ENDIAN
    array_u64[bank_nr][idx] = __builtin_bswap64(data);
#else
    array_u64[bank_nr][idx] = data;
#endif
}

// Write a quad, interleaved banks, little-endian
void SDRAM::write_u64_i_le(vluint32_t addr, vluint64_t data)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |      rows      |  banks  |    columns     |  data   |
    // |<-- bit_rows -->|<-- 2 -->|<-- bit_cols -->|<-- 3 -->|
    addr  >>= 3;
    bank_nr = (addr & mask_bank) >> bit_cols;
    idx     = (addr & mask_cols) | ((addr & mask_rows) >> SDRAM_BIT_BANKS);

#if BYTE_ORDER == BIG_ENDIAN
    array_u64[bank_nr][idx] = __builtin_bswap64(data);
#else
    array_u64[bank_nr][idx] = data;
#endif
}

// Write a byte, contiguous banks
void SDRAM::write_u8_c(vluint32_t addr, vluint8_t data)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |  banks  |      rows      |    columns     |
    // |<-- 2 -->|<-- bit_rows -->|<-- bit_cols -->|
    bank_nr = (addr & mask_bank) >> (bit_cols + bit_rows);
    idx     = (addr & (mask_cols | mask_rows));

    array_u8[bank_nr][idx] = data;
}

// Write a word, contiguous banks, big-endian
void SDRAM::write_u16_c_be(vluint32_t addr, vluint16_t data)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |  banks  |      rows      |    columns     |  data   |
    // |<-- 2 -->|<-- bit_rows -->|<-- bit_cols -->|<-- 1 -->|
    addr  >>= 1;
    bank_nr = (addr & mask_bank) >> (bit_cols + bit_rows);
    idx     = (addr & (mask_cols | mask_rows));

#if BYTE_ORDER == LITTLE_ENDIAN
    array_u16[bank_nr][idx] = __builtin_bswap16(data);
#else
    array_u16[bank_nr][idx] = data;
#endif
}

// Write a word, contiguous banks, little-endian
void SDRAM::write_u16_c_le(vluint32_t addr, vluint16_t data)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |  banks  |      rows      |    columns     |  data   |
    // |<-- 2 -->|<-- bit_rows -->|<-- bit_cols -->|<-- 1 -->|
    addr  >>= 1;
    bank_nr = (addr & mask_bank) >> (bit_cols + bit_rows);
    idx     = (addr & (mask_cols | mask_rows));

#if BYTE_ORDER == BIG_ENDIAN
    array_u16[bank_nr][idx] = __builtin_bswap16(data);
#else
    array_u16[bank_nr][idx] = data;
#endif
}

// Write a long, contiguous banks, big-endian
void SDRAM::write_u32_c_be(vluint32_t addr, vluint32_t data)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |  banks  |      rows      |    columns     |  data   |
    // |<-- 2 -->|<-- bit_rows -->|<-- bit_cols -->|<-- 2 -->|
    addr  >>= 2;
    bank_nr = (addr & mask_bank) >> (bit_cols + bit_rows);
    idx     = (addr & (mask_cols | mask_rows));

#if BYTE_ORDER == LITTLE_ENDIAN
    array_u32[bank_nr][idx] = __builtin_bswap32(data);
#else
    array_u32[bank_nr][idx] = data;
#endif
}

// Write a long, contiguous banks, little-endian
void SDRAM::write_u32_c_le(vluint32_t addr, vluint32_t data)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |  banks  |      rows      |    columns     |  data   |
    // |<-- 2 -->|<-- bit_rows -->|<-- bit_cols -->|<-- 2 -->|
    addr  >>= 2;
    bank_nr = (addr & mask_bank) >> (bit_cols + bit_rows);
    idx     = (addr & (mask_cols | mask_rows));

#if BYTE_ORDER == BIG_ENDIAN
    array_u32[bank_nr][idx] = __builtin_bswap32(data);
#else
    array_u32[bank_nr][idx] = data;
#endif
}

// Write a quad, contiguous banks, big-endian
void SDRAM::write_u64_c_be(vluint32_t addr, vluint64_t data)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |  banks  |      rows      |    columns     |  data   |
    // |<-- 2 -->|<-- bit_rows -->|<-- bit_cols -->|<-- 3 -->|
    addr  >>= 3;
    bank_nr = (addr & mask_bank) >> (bit_cols + bit_rows);
    idx     = (addr & (mask_cols | mask_rows));

#if BYTE_ORDER == LITTLE_ENDIAN
    array_u64[bank_nr][idx] = __builtin_bswap64(data);
#else
    array_u64[bank_nr][idx] = data;
#endif
}

// Write a quad, contiguous banks, little-endian
void SDRAM::write_u64_c_le(vluint32_t addr, vluint64_t data)
{
    vluint32_t bank_nr;  // Bank number (0 to 3)
    vluint32_t idx;      // Array index (0 to num_cols * num_rows - 1)
    
    // |  banks  |      rows      |    columns     |  data   |
    // |<-- 2 -->|<-- bit_rows -->|<-- bit_cols -->|<-- 3 -->|
    addr  >>= 3;
    bank_nr = (addr & mask_bank) >> (bit_cols + bit_rows);
    idx     = (addr & (mask_cols | mask_rows));

#if BYTE_ORDER == BIG_ENDIAN
    array_u64[bank_nr][idx] = __builtin_bswap64(data);
#else
    array_u64[bank_nr][idx] = data;
#endif
}
