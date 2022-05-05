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

#ifndef _UART_IF_H_
#define _UART_IF_H_

#include "verilated.h"
#include <queue>

#define RX_OK_START    (1)
#define RX_OK          (0)
#define RX_EMPTY       (-1)
#define RX_PARITY_ERR  (-2)
#define RX_FRAMING_ERR (-3)

class UartIF
{
    public:
        // Constructor and destructor
        UartIF();
        ~UartIF();
        // Methods
        void        Eval(vluint8_t bclk);
        vluint64_t  SetUartConfig(const char *uart_cfg, vluint32_t baud, short inter_byte);
        void        SetRxTimeout(vluint32_t timeout_us);
        void        ConnectTx(vluint8_t *sig);
        void        ConnectRx(vluint8_t *sig);
        void        PutTxChar(vluint16_t data);
        void        PutTxString(const char *str);
        bool        IsRxEmpty(void);
        int         RxSize(void);
        int         GetRxChar(vluint16_t &data);
        void        SetTXE_CallBack(void (*cback)());
        void        SetRXF_CallBack(void (*cback)(), int level);
    private:
        // Private methods
        vluint16_t  CalcParity(vluint16_t data);
        // Parity configuration
        enum par_cfg_t
        {
            PARITY_NONE = 0,
            PARITY_ODD  = 1,
            PARITY_EVEN = 2
        };
        
        // Stop bits masks definitions
        const vluint16_t STOP_MSK_8N1  = 0b0000000100000000;
        const vluint16_t STOP_MSK_8N2  = 0b0000001100000000;
        const vluint16_t STOP_MSK_9N1  = 0b0000001000000000;
        const vluint16_t STOP_MSK_9N2  = 0b0000011000000000;
        const vluint16_t STOP_MSK_8P1  = 0b0000001000000000;
        const vluint16_t STOP_MSK_8P2  = 0b0000011000000000;
        const vluint16_t STOP_MSK_9P1  = 0b0000010000000000;
        const vluint16_t STOP_MSK_9P2  = 0b0000110000000000;
        
        // RX data bit masks definitions
        const vluint16_t RXD_MSK_8N1   = 0b1111110111111111;
        const vluint16_t RXD_MSK_8N2   = 0b1111101111111111;
        const vluint16_t RXD_MSK_9N1   = 0b1111101111111111;
        const vluint16_t RXD_MSK_9N2   = 0b1111011111111111;
        const vluint16_t RXD_MSK_8P1   = 0b1111101111111111;
        const vluint16_t RXD_MSK_8P2   = 0b1111011111111111;
        const vluint16_t RXD_MSK_9P1   = 0b1111011111111111;
        const vluint16_t RXD_MSK_9P2   = 0b1110111111111111;
        
        const vluint16_t DATA_MSK_8B   = 0b0000000011111111;
        const vluint16_t DATA_MSK_9B   = 0b0000000111111111;
        const vluint16_t TX_DATA_EMPTY = 0b0000000000000000;
        const vluint16_t RX_DATA_EMPTY = 0b1111111111111111;
        const vluint16_t RX_START      = 0b1000000000000000;
        const vluint16_t RX_STOP_OK    = 0b0100000000000000;
        const vluint16_t RX_PARITY_OK  = 0b0010000000000000;
        
        const vluint32_t UART_BAUD_MIN  = 1200;
        const vluint32_t UART_BAUD_DFT  = 115200;
        
        // Clock period (in ps)
        vluint64_t  m_baudClkPer;
        // UART baud rate
        vluint32_t  m_baudRate;
        // 8-bit (false) or 9-bit (true) mode
        bool        m_9bitMode;
        // Parity configuration
        par_cfg_t   m_parity;
        // Stop bits mask
        vluint16_t  m_stopBits;
        // RX data bit mask
        vluint16_t  m_rxBitMask;
        // Data bits mask
        vluint16_t  m_dataMask;
        // Inter byte delay
        short       m_txInterByte;
        // Current transmit cycle
        short       m_txCycle;
        // Data being transmitted
        vluint16_t  m_txData;
        // Current receive state
        short       m_rxCycle;
        // Data being received
        vluint16_t  m_rxData;
        // Tx buffer
        std::queue<vluint16_t> m_txBuffer;
        // Rx buffer
        std::queue<vluint16_t> m_rxBuffer;
        // Uart TX signal
        vluint8_t  *m_txSignal;
        // Uart TX empty call-back
        void      (*m_txeCback)();
        // Uart RX signal
        vluint8_t  *m_rxSignal;
        // Uart RX full call-back
        void      (*m_rxfCback)();
        // Uart RX full level
        int         m_rxLevel;
        // RX time-out management
        vluint32_t  m_rxTimeoutVal;  // RX timeout value (in cycles)
        vluint32_t  m_rxTimeoutCtr;  // RX timeout counter (in cycles)
        bool        m_rxTimeout;
        // UART internal loopback signal
        vluint8_t   m_loopBackSignal;
        // Previous baud clock value
        vluint8_t   m_prevBaudClk;
        // Previous RX pin value
        vluint8_t   m_prevRxSignal;
};

#endif /* _UART_IF_H_ */
