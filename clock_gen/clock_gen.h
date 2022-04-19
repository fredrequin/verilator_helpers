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
// Clock generator:
// ----------------
//  - Designed to work with "Verilator" tool (www.veripool.org)
//  - Allow any number of clocks
//  - Arbitrary clocks' periods and phase
//  - Clocks can be started/stopped
//  - Clocks can be directly connected to a signal
//  - Event list management
//  - Simulation progress in us when quiet mode is off

#ifndef _CLOCK_GEN_H_
#define _CLOCK_GEN_H_

#include "verilated.h"
#include <queue>

// Helper macros for timestamps
#define TS_NS(ts) (1000LL*ts)
#define TS_US(ts) (1000000LL*ts)
#define TS_MS(ts) (1000000000LL*ts)
#define TS_S(ts)  (1000000000000LL*ts)

class ClockGen
{
    public:
        // Constructor and destructor
        ClockGen(int num_clk);
        ~ClockGen();
        // Methods
        void        AddEvent(vluint64_t stamp_ps, void (*cback)());
        void        NewClock(int idx, vluint64_t period_ps);
        void        ConnectClock(int idx, vluint8_t *sig);
        void        StartClock(int idx, vluint64_t stamp_ps);
        void        StartClock(int idx, vluint64_t phase_ps, vluint64_t stamp_ps);
        void        StopClock(int idx);
        vluint8_t   GetClockStateDiv1(int idx, vluint8_t phase); // phase : 0 - 1
        vluint8_t   GetClockStateDiv2(int idx, vluint8_t phase); // phase : 0 - 3
        vluint8_t   GetClockStateDiv4(int idx, vluint8_t phase); // phase : 0 - 7
        vluint8_t   GetClockStateDiv8(int idx, vluint8_t phase); // phase : 0 - 15
        vluint8_t   GetClockStateDiv16(int idx, vluint8_t phase); // phase : 0 - 31
        vluint8_t   GetClockStateDiv32(int idx, vluint8_t phase); // phase : 0 - 63
        void        AdvanceClocks(vluint64_t &stamp_ps, bool quiet);
    private:
        // Clock type
        typedef struct
        {
            vluint64_t clk_stamp_ps; // Clock's time stamps (in ps)
            vluint8_t *clk_sig;      // Clock signal address
            vluint32_t clk_hper_ps;  // Clock's half period (in ps)
            vluint8_t  clk_state;    // Clock's state (0 - 255)
            vluint8_t  clk_dummy;    // Dummy clock signal
            bool       clk_enable;   // Clock enabled
        } vl_clk_t;
        
        // Clock list type
        typedef std::vector
        <
            vl_clk_t
        > vl_clk_list_t;
        
        // Event type
        typedef struct
        {
            vluint64_t evt_stamp_ps; // Event's time stamps (in ps)
            void     (*evt_cback)(); // Event's call back function
        } vl_evt_t;
        
        // Custom compare function for std::priority_queue
        class vl_evt_compare
        {
            public:
                bool operator() (const vl_evt_t &lhs, const vl_evt_t &rhs)
                {
                    return lhs.evt_stamp_ps > rhs.evt_stamp_ps;
                }
        };
        
        // Event list type
        typedef std::priority_queue
        <
            vl_evt_t,
            std::vector<vl_evt_t>,
            vl_evt_compare
        > vl_evt_list_t;
        
        const int      m_clockMax;      // Number of clocks
        vluint64_t     m_maxStep_ps;    // Maximum simulation step (in ps)
        vluint64_t     m_nextStamp_ps;  // Next time stamp (in ps)
        vl_clk_list_t  m_clockList;     // Clocks list
        vl_evt_t       m_event;         // Current event
        vl_evt_list_t  m_eventList;     // Events list
};

#endif /* _CLOCK_GEN_H_ */
