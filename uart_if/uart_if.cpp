// Copyright 2019-2022 Frederic Requin
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
// UART interface:
// ---------------
//  - Designed to work with "Verilator" tool (www.veripool.org)
//  - UART Rx and Tx management with data FIFOs
//  - Rx and Tx can be directly connected to testbench signals
//  - Baud rate generation works with the clock generator
//  - 8-bit or 9-bit data
//  - Odd/Even/No parity modes
//  - 1 or 2 stop bits

#include "uart_if.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// Constructor
UartIF::UartIF()
{
    // No UART connection
    prev_bclk    = 0;
    prev_rx      = 1;
    
    // Initialize time variables
    baud_clk_per = (vluint64_t)200000000000LL / UART_BAUD_DFT;
    
    // Default UART configuration (8N1 @ 115200 bauds)
    baud_rate    = UART_BAUD_DFT;
    mode_9bit    = false;         // 8
    parity       = PARITY_NONE;   // N
    stop_bits    = STOP_MSK_8N1;  // 1
    rx_bit_msk   = RXD_MSK_8N1;
    data_msk     = DATA_MSK_8B;
    tx_ib_dly    = 3;             // 3/5 bit delay
    
    // TX state
    tx_data      = TX_DATA_EMPTY;
    tx_cycle     = -3;
    tx_sig       = NULL;
    
    // RX state
    rx_data      = RX_DATA_EMPTY;
    rx_cycle     = 0;
    rx_sig       = NULL;
}

// Destructor
UartIF::~UartIF()
{
}

// Configure the UART
vluint64_t UartIF::SetUartConfig(const char *uart_cfg, vluint32_t baud, short inter_byte)
{
    const vluint16_t c_stop_mask[8] =
    {
        STOP_MSK_8N1, STOP_MSK_8N2, STOP_MSK_8P1, STOP_MSK_8P2,
        STOP_MSK_9N1, STOP_MSK_9N2, STOP_MSK_9P1, STOP_MSK_9P2
    };
    const vluint16_t c_rxd_mask[8] =
    {
        RXD_MSK_8N1,  RXD_MSK_8N2,  RXD_MSK_8P1,  RXD_MSK_8P2,
        RXD_MSK_9N1,  RXD_MSK_9N2,  RXD_MSK_9P1,  RXD_MSK_9P2
    };
    
    int cfg_idx;
    
    // Boundary check
    if (strlen(uart_cfg) != 3)
    {
        printf("UART : bad configuration string !!\n");
        fflush(stdout);
        return 0LL;
    }
    if (baud < UART_BAUD_MIN)
    {
        printf("UART : baud rate too low !!\n");
        fflush(stdout);
        return 0LL;
    }
    
    // Stop bits config
    switch (uart_cfg[2])
    {
        case '1' : cfg_idx = 0; break;
        case '2' : cfg_idx = 1; break;
        default :
        {
            printf("UART : wrong number of stop bits !!\n");
            fflush(stdout);
            return 0LL;
        }
    }
    
    // Parity config
    switch (uart_cfg[1])
    {
        case 'N' : parity = PARITY_NONE;               break;
        case 'O' : parity = PARITY_ODD;  cfg_idx += 2; break;
        case 'E' : parity = PARITY_EVEN; cfg_idx += 2; break;
        default  : 
        {
            printf("UART : invalid parity mode !!\n");
            fflush(stdout);
            return 0LL;
        }
    }
    
    // Data bits config
    switch (uart_cfg[0])
    {
        case '8' : mode_9bit = false;               break;
        case '9' : mode_9bit = true;  cfg_idx += 4; break;
        default  :
        {
            printf("UART : wrong number of data bits !!\n");
            fflush(stdout);
            return 0LL;
        }
    }
    
    // Stop bits mask
    stop_bits    = c_stop_mask[cfg_idx];
    
    // Receive bit mask
    rx_bit_msk   = c_rxd_mask[cfg_idx];
    
    // Data bits mask
    data_msk     = (mode_9bit) ? DATA_MSK_9B : DATA_MSK_8B;
    
    // Baud rate config
    baud_rate    = baud;
    
    // Baud clock : 5x over-sampling
    baud_clk_per = (vluint64_t)200000000000LL / baud;
    
    // Inter byte delay in bit clock cycles
    tx_ib_dly    = inter_byte;
    
    return baud_clk_per;
}

// Connect the UART TX to a signal
void UartIF::ConnectTx(vluint8_t *sig)
{
    // Store the signal's memory address
    tx_sig = sig;
    // Set TX in idle state
    *tx_sig = 1;
}

// Connect the UART RX to a signal
void UartIF::ConnectRx(vluint8_t *sig)
{
    // Store the signal's memory address
    rx_sig = sig;
    // We assume RX is in idle state
    prev_rx = 1;
}

// Write one data into the TX buffer
void UartIF::PutTxChar(vluint16_t data)
{
    tx_buf.push(data & data_msk);
}

void UartIF::PutTxString(const char *str)
{
    while (*str) tx_buf.push((vluint16_t)*str++);
}

// Read one data from the RX buffer
int  UartIF::GetRxChar(vluint16_t &data)
{
    if (rx_buf.empty())
    {
        data = 0;
        return RX_EMPTY;
    }
    else
    {
        vluint16_t tmp;
        
        tmp = rx_buf.front();
        rx_buf.pop();
        data = tmp & data_msk;
        
        if (tmp & RX_STOP_OK)
        {
            if (tmp & RX_PARITY_OK)
            {
                return RX_OK;
            }
            else
            {
                return RX_PARITY_ERR;
            }
        }
        else
        {
            return RX_FRAMING_ERR;
        }
    }
}

// Evaluate TX and RX channels
void UartIF::Eval(vluint8_t bclk)
{
    // Baud clock rising edge
    if (bclk && !prev_bclk)
    {
        // TX is busy
        if (tx_data)
        {
            // Every 5 cycles, shift one bit out
            if (tx_cycle == 4)
            {
                // Least significant bit first
                tx_data >>= 1;
                if (tx_data)
                {
                    // Shift one bit out
                    *tx_sig = tx_data & 1;
                    // Restart cycle counter
                    tx_cycle = 0;
                }
                else
                {
                    // Set inter byte delay
                    tx_cycle = -tx_ib_dly;
                }
            }
            else
            {
                tx_cycle++;
            }
        }
        // TX is idling
        if (!tx_data)
        {
            // Manage the inter-byte delay
            if (tx_cycle < 0)
            {
                tx_cycle++;
            }
            // Prepare a new character (if available)
            else
            {
                if (!tx_buf.empty())
                {
                    // Get one byte from the buffer
                    tx_data = tx_buf.front();
                    tx_buf.pop();
                    // Add parity
                    tx_data |= CalcParity(tx_data);
                    // Add stop bits
                    tx_data |= stop_bits;
                    // Send START bit first
                    tx_data <<= 1;
                    *tx_sig = 0;
                }
            }
        }
        
        // Receive one character (one bit every 5 cycles)
        if (rx_cycle)
        {
            // Middle of the bit time : Shift one bit in
            if (rx_cycle == 3)
            {
                // By default, shift a one
                rx_data = (rx_data >> 1) | (vluint16_t)0x8000;
                // Shift a zero if RX pin = 0
                if (*rx_sig == 0) rx_data &= rx_bit_msk;
            }
            // Count cycles
            if (rx_cycle == 5)
            {
                rx_cycle = rx_data & 1;
            }
            else
            {
                rx_cycle++;
            }
            
            // Check if START bit is in bit #0
            if (rx_cycle == 0)
            {
                vluint16_t tmp;
                
                // Drop START bit
                rx_data >>= 1;
                // Check parity bit
                if (parity)
                {
                    tmp = (mode_9bit) ? rx_data & 0x200 : rx_data & 0x100;
                    tmp = (tmp == CalcParity(rx_data)) ? RX_PARITY_OK : 0;
                }
                else
                {
                    tmp = RX_PARITY_OK;
                }
                // Check stop bits
                if ((rx_data & stop_bits) == stop_bits) tmp |= RX_STOP_OK;
                // Extract data bits
                tmp |= rx_data & data_msk;
                // Store result
                rx_buf.push(tmp);
                // Clear RX buffer
                rx_data = RX_DATA_EMPTY;
            }
        }
        // Wait for a new character
        else
        {
            // RX falling edge (START bit)
            if (prev_rx && !(*rx_sig))
            {
                // Activate RX state machine
                rx_cycle = 1;
            }
        }
        // Previous RX value
        prev_rx = *rx_sig;
    }
    // Previous baud clock value
    prev_bclk = bclk;
}

// Compute even/odd parity on an 8/9-bit data
vluint16_t UartIF::CalcParity(vluint16_t data)
{
    vluint16_t tmp = 0x0000;

    // No parity case
    if (parity == PARITY_NONE) return tmp;
    
    // ({ data[7:0], 1'b0 } ^ { data[6:0], 2'b0 }) & 9'b101010100
    tmp = ((data << 1) ^ (data << 2)) & 0x154;
    // (tmp[8:0] ^ { data[6:0], 2'b0 }) & 9'b100010000
    tmp = (tmp ^ (tmp << 2)) & 0x110;
    // (tmp[8:0] ^ { data[4:0], 4'b0 }) & 9'b100000000
    tmp = (tmp ^ (tmp << 4)) & 0x100;

    // Odd parity case
    if (parity == PARITY_ODD) tmp ^= 0x100;
    
    // 9-bit case
    if (mode_9bit)
        return (tmp ^ (data & 0x100)) << 1;
    // 8-bit case
    else
        return tmp;
}
