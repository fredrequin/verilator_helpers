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
#include <limits.h>

// Constructor
UartIF::UartIF() :
    // No UART connection
    m_loopBackSignal { (vluint8_t)1 },
    m_prevBaudClk    { (vluint8_t)0 },
    m_prevRxSignal   { (vluint8_t)1 },
    // Initialize time variables
    m_baudClkPer     { (vluint64_t)200000000000UL / UART_BAUD_DFT },
    m_rxTimeoutVal   {       (vluint32_t)10000000 / UART_BAUD_DFT },
    m_rxTimeoutCtr   {                              (vluint32_t)0 },
    m_rxTimeout      {                                      false },
    // Default UART configuration (8N1 @ 115200 bauds)
    m_baudRate       {     UART_BAUD_DFT },
    m_9bitMode       {             false }, // 8
    m_parity         {       PARITY_NONE }, // N
    m_stopBits       {      STOP_MSK_8N1 }, // 1
    m_rxBitMask      {       RXD_MSK_8N1 },
    m_dataMask       {       DATA_MSK_8B },
    m_txInterByte    {                 3 }, // 3/5 bit delay
    // TX state
    m_txData         {     TX_DATA_EMPTY },
    m_txCycle        {                -3 },
    m_txSignal       { &m_loopBackSignal },
    m_txeCback       {              NULL },
    // RX state
    m_rxData         {     RX_DATA_EMPTY },
    m_rxCycle        {                 0 },
    m_rxSignal       { &m_loopBackSignal },
    m_rxtoCback      {              NULL },
    m_rxfCback       {              NULL },
    m_rxLevel        {           INT_MAX }
{
}

// Destructor
UartIF::~UartIF()
{
    // Flush the buffers
    while (!m_rxBuffer.empty()) m_rxBuffer.pop();
    while (!m_txBuffer.empty()) m_txBuffer.pop();
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
        return 0UL;
    }
    if (baud < UART_BAUD_MIN)
    {
        printf("UART : baud rate too low !!\n");
        fflush(stdout);
        return 0UL;
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
            return 0UL;
        }
    }
    
    // Parity config
    switch (uart_cfg[1])
    {
        case 'N' : m_parity = PARITY_NONE;               break;
        case 'O' : m_parity = PARITY_ODD;  cfg_idx += 2; break;
        case 'E' : m_parity = PARITY_EVEN; cfg_idx += 2; break;
        default  : 
        {
            printf("UART : invalid parity mode !!\n");
            fflush(stdout);
            return 0UL;
        }
    }
    
    // Data bits config
    switch (uart_cfg[0])
    {
        case '8' : m_9bitMode = false;               break;
        case '9' : m_9bitMode = true;  cfg_idx += 4; break;
        default  :
        {
            printf("UART : wrong number of data bits !!\n");
            fflush(stdout);
            return 0UL;
        }
    }
    
    // Stop bits mask
    m_stopBits    = c_stop_mask[cfg_idx];
    
    // Receive bit mask
    m_rxBitMask   = c_rxd_mask[cfg_idx];
    
    // Data bits mask
    m_dataMask    = (m_9bitMode) ? DATA_MSK_9B : DATA_MSK_8B;
    
    // Baud rate config
    m_baudRate    = baud;
    
    // Baud clock : 5x over-sampling
    m_baudClkPer  = (vluint64_t)200000000000UL / baud;
    
    // Inter byte delay in bit clock cycles
    m_txInterByte = inter_byte;
    
    return m_baudClkPer;
}

// Set RX time-out
void UartIF::SetRxTimeout(vluint32_t timeout_us)
{
    if (timeout_us < ((vluint32_t)1000000 / m_baudRate))
    {
        printf("UART : RX timeout too low !!\n");
        fflush(stdout);
        return;
    }
    // Timeout delays (us -> cycles)
    m_rxTimeoutVal = (vluint32_t)(((vluint64_t)1000000UL * timeout_us) / m_baudClkPer);
    m_rxTimeoutCtr = (vluint32_t)0;
    m_rxTimeout    = false;
}

// Connect the UART TX to a signal
void UartIF::ConnectTx(vluint8_t *sig)
{
    // Store the signal's memory address
    m_txSignal = sig;
    // Set TX in idle state
    *m_txSignal = (vluint8_t)1;
}

// Connect the UART RX to a signal
void UartIF::ConnectRx(vluint8_t *sig)
{
    // Store the signal's memory address
    m_rxSignal     = sig;
    // We assume RX is in idle state
    m_prevRxSignal = (vluint8_t)1;
}

// Write one data into the TX buffer
void UartIF::PutTxChar(vluint16_t data)
{
    m_txBuffer.push(data & m_dataMask);
}

void UartIF::PutTxString(const char *str)
{
    while (*str) m_txBuffer.push((vluint16_t)*str++);
}

// Read one data from the RX buffer
int  UartIF::GetRxChar(vluint16_t &data)
{
    if (m_rxBuffer.empty())
    {
        data = 0;
        return RX_EMPTY;
    }
    else
    {
        vluint16_t tmp;
        
        tmp = m_rxBuffer.front();
        //printf("%04X ", tmp);
        m_rxBuffer.pop();
        data = tmp & m_dataMask;
        
        if (tmp & RX_STOP_OK)
        {
            if (tmp & RX_PARITY_OK)
            {
                if (tmp & RX_START)
                {
                    return RX_OK_START;
                }
                else
                {
                    return RX_OK;
                }
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

void UartIF::SetTXE_CallBack(void (*cback)())
{
    m_txeCback = cback;
}

void UartIF::SetRXT_CallBack(void (*cback)())
{
    m_rxtoCback    = cback;
    m_rxTimeoutCtr = (vluint32_t)0;
    m_rxTimeout    = false;
}

void UartIF::SetRXF_CallBack(void (*cback)(), int level)
{
    if (cback)
    {
        m_rxfCback = cback;
        m_rxLevel  = (level > 0) ? level : 1;
    }
    else
    {
        m_rxfCback = NULL;
        m_rxLevel  = INT_MAX;
    }
}

// Evaluate TX and RX channels
void UartIF::Eval(vluint8_t bclk)
{
    // Baud clock rising edge
    if (bclk && !m_prevBaudClk)
    {
        // TX is busy
        if (m_txData)
        {
            // Every 5 cycles, shift one bit out
            if (m_txCycle == 4)
            {
                // Least significant bit first
                m_txData >>= 1;
                if (m_txData)
                {
                    // Shift one bit out
                    *m_txSignal = m_txData & 1;
                    // Restart cycle counter
                    m_txCycle = 0;
                }
                else
                {
                    // Set inter byte delay
                    m_txCycle = -m_txInterByte;
                    // TX buffer empty call-back
                    if (m_txBuffer.empty() && (m_txeCback))
                    {
                        m_txeCback();
                    }
                }
            }
            else
            {
                m_txCycle++;
            }
        }
        // TX is idling
        if (!m_txData)
        {
            // Manage the inter-byte delay
            if (m_txCycle < 0)
            {
                m_txCycle++;
            }
            // Prepare a new character (if available)
            else
            {
                if (!m_txBuffer.empty())
                {
                    // Get one byte from the buffer
                    m_txData = m_txBuffer.front();
                    m_txBuffer.pop();
                    // Add parity
                    m_txData |= CalcParity(m_txData);
                    // Add stop bits
                    m_txData |= m_stopBits;
                    // Send START bit first
                    m_txData <<= 1;
                    *m_txSignal = 0;
                }
            }
        }
        
        // Receive one character (one bit every 5 cycles)
        if (m_rxCycle)
        {
            // Middle of the bit time : Shift one bit in
            if (m_rxCycle == 3)
            {
                // By default, shift a one
                m_rxData = (m_rxData >> 1) | (vluint16_t)0b1000000000000000;
                // Shift a zero if RX pin = 0
                if (*m_rxSignal == 0) m_rxData &= m_rxBitMask;
            }
            // Full byte received ?
            if (m_rxData & 1)
            {
                // No, count cycles
                m_rxCycle = (m_rxCycle == 5) ? 1 : m_rxCycle + 1;
            }
            else
            {
                vluint16_t tmp;
                
                // Yes, decode byte
                m_rxCycle = 0;
                
                // Drop START bit
                m_rxData >>= 1;
                // Check parity bit
                if (m_parity)
                {
                    tmp = (m_9bitMode) ? m_rxData & 0b1000000000 : m_rxData & 0b100000000;
                    tmp = (tmp == CalcParity(m_rxData)) ? RX_PARITY_OK : 0;
                }
                else
                {
                    tmp = RX_PARITY_OK;
                }
                // Check stop bits
                if ((m_rxData & m_stopBits) == m_stopBits) tmp |= RX_STOP_OK;
                // Mark start of message
                if (m_rxTimeout)
                {
                    tmp |= RX_START;
                    m_rxTimeout = false;
                }
                // Extract data bits
                tmp |= m_rxData & m_dataMask;
                // Store result
                m_rxBuffer.push(tmp);
                // Clear RX buffer
                m_rxData = RX_DATA_EMPTY;
                // RX buffer full call-back
                if (m_rxBuffer.size() >= m_rxLevel)
                {
                    m_rxfCback();
                }
            }
        }
        // Wait for a new character
        else
        {
            // RX falling edge (START bit)
            if (m_prevRxSignal && !(*m_rxSignal))
            {
                // Clear the time-out counter
                m_rxTimeoutCtr = 0;
                // Activate RX state machine
                m_rxCycle = 1;
            }
            else
            {
                // Time-out counter management
                if (!m_rxTimeout)
                {
                    m_rxTimeoutCtr ++;
                    m_rxTimeout = (m_rxTimeoutCtr >= m_rxTimeoutVal);
                    // Time-out call-back for error management
                    if (m_rxTimeout && (m_rxtoCback))
                    {
                        m_rxtoCback();
                    }
                }
            }
        }
        // Previous RX value
        m_prevRxSignal = *m_rxSignal;
    }
    // Previous baud clock value
    m_prevBaudClk = bclk;
}

// Compute even/odd parity on an 8/9-bit data
vluint16_t UartIF::CalcParity(vluint16_t data)
{
    vluint16_t tmp = (vluint16_t)0;

    // No parity case
    if (m_parity == PARITY_NONE) return tmp;
    
    // ({ data[7:0], 1'b0 } ^ { data[6:0], 2'b0 }) & 9'b101010100
    tmp = ((data << 1) ^ (data << 2)) & 0b101010100;
    // (tmp[8:0] ^ { data[6:0], 2'b0 }) & 9'b100010000
    tmp = (tmp ^ (tmp << 2)) & 0b100010000;
    // (tmp[8:0] ^ { data[4:0], 4'b0 }) & 9'b100000000
    tmp = (tmp ^ (tmp << 4)) & 0b100000000;

    // Odd parity case
    if (m_parity == PARITY_ODD) tmp ^= 0b100000000;
    
    // 9-bit case
    if (m_9bitMode)
        return (tmp ^ (data & 0b100000000)) << 1;
    // 8-bit case
    else
        return tmp;
}
