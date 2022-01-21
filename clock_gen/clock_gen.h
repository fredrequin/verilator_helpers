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
//  - Event list management (experimental)
//  - Simulation progress in us when quiet mode is off

#ifndef _CLOCK_GEN_H_
#define _CLOCK_GEN_H_

#include "verilated.h"

// Helper macros for timestamps
#define TS_NS(ts) (1000LL*ts)
#define TS_US(ts) (1000000LL*ts)
#define TS_MS(ts) (1000000000LL*ts)
#define TS_S(ts)  (1000000000000LL*ts)

class ClockGen
{
    public:
        // Constructor and destructor
        ClockGen(int num_clk, int evt_depth);
        ~ClockGen();
        // Methods
        void        AddEvent(vluint64_t stamp_ps, void (*cback)());
        void        NewClock(int clk_idx, vluint64_t period_ps);
        void        ConnectClock(int clk_idx, vluint8_t *sig);
        void        StartClock(int clk_idx, vluint64_t stamp_ps);
        void        StartClock(int clk_idx, vluint64_t phase_ps, vluint64_t stamp_ps);
        void        StopClock(int clk_idx);
        vluint8_t   GetClockStateDiv1(int clk_idx, vluint8_t phase); // phase : 0 - 1
        vluint8_t   GetClockStateDiv2(int clk_idx, vluint8_t phase); // phase : 0 - 3
        vluint8_t   GetClockStateDiv4(int clk_idx, vluint8_t phase); // phase : 0 - 7
        vluint8_t   GetClockStateDiv8(int clk_idx, vluint8_t phase); // phase : 0 - 15
        vluint8_t   GetClockStateDiv16(int clk_idx, vluint8_t phase); // phase : 0 - 31
        vluint8_t   GetClockStateDiv32(int clk_idx, vluint8_t phase); // phase : 0 - 63
        void        AdvanceClocks(vluint64_t &stamp_ps, bool quiet);
    private:
        typedef struct
        {
            vluint64_t clk_stamp_ps; // Clock's time stamps (in ps)
            vluint8_t *clk_sig;      // Clock signal address
            vluint32_t clk_hper_ps;  // Clock's half period (in ps)
            vluint8_t  clk_state;    // Clock's state (0 - 255)
            vluint8_t  clk_dummy;    // Dummy clock signal
            bool       clk_enable;   // Clock enabled
        } vlclk_t;
        typedef struct
        {
            vluint64_t evt_stamp_ps; // Event's time stamps (in ps)
            void     (*evt_cback)(); // Event's call back function
        } vlevt_t;
        int         num_clock;       // Number of clocks
        int         event_depth;     // Event queue depth
        int         event_stored;    // Number of events stored
        int         event_wr_idx;    // Event queue write index
        int         event_rd_idx;    // Event queue read index
        vluint64_t  max_step_ps;     // Maximum simulation step (in ps)
        vluint64_t  next_stamp_ps;   // Next time stamp (in ps)
        vlclk_t    *clk_map;         // Created clocks
        vlevt_t    *evt_buf;         // Event queue
        vlevt_t    *evt_ptr;         // Current event
};

#endif /* _CLOCK_GEN_H_ */
