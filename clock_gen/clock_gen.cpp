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

#include "verilated.h"
#include "clock_gen.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// Constructor : set the number of clocks
ClockGen::ClockGen(int num_clk, int evt_depth)
{
    // Initialize time variables
    max_step_ps    = (vluint64_t)0;
    next_stamp_ps  = (vluint64_t)0;
    
    // Allocate the clocks
    num_clock      = num_clk;
    clk_map        = new vlclk_t[num_clk];
    
    // Clear the clocks
    vlclk_t *pc = clk_map;
    for (int i = 0; i < num_clk; i++)
    {
        pc->clk_stamp_ps = (vluint64_t)0;
        pc->clk_sig      = &pc->clk_dummy;
        pc->clk_hper_ps  = (vluint32_t)0;
        pc->clk_state    = (vluint8_t)0;
        pc->clk_dummy    = (vluint8_t)0;
        pc->clk_enable   = false;
        pc++;
    }
    
    // Allocate the events
    event_depth    = evt_depth;
    evt_buf        = new vlevt_t[evt_depth];
    evt_ptr        = &evt_buf[0];
    
    event_stored   = 0;
    event_wr_idx   = 0;
    event_rd_idx   = 0;
    
    // Clear the events
    vlevt_t *pe = evt_buf;
    for (int i = 0; i < evt_depth; i++)
    {
        pe->evt_stamp_ps = (vluint64_t)-1;
        pe->evt_cback    = NULL;
        pe++;
    }
}

// Destructor
ClockGen::~ClockGen()
{
    delete [] clk_map;
    delete [] evt_buf;
}

// Add an event
void ClockGen::AddEvent(vluint64_t stamp_ps, void (*cback)())
{
    // Buffer overflow : exit
    if (event_stored >= event_depth) return;
    // Store the event
    evt_buf[event_wr_idx].evt_stamp_ps = stamp_ps;
    evt_buf[event_wr_idx].evt_cback  = cback;
    // Point to the next event location
    if ((++event_wr_idx) == event_depth) event_wr_idx = 0;
    event_stored++;
}

// Create a new clock
void ClockGen::NewClock(int clk_idx, vluint64_t period_ps)
{
    // Boundary check
    if (clk_idx >= num_clock) return;
    // Store the clock's half period
    clk_map[clk_idx].clk_hper_ps = (vluint32_t)(period_ps >> 1);
    // Adjust the maximum simulation step
    if (max_step_ps < (period_ps >> 1))
    {
        max_step_ps = (period_ps >> 1) + 1;
    }
}

// Connect the undivided clock to a signal
void ClockGen::ConnectClock(int clk_idx, vluint8_t *sig)
{
    // Boundary check
    if (clk_idx >= num_clock) return;
    // Store the signal's memory address
    clk_map[clk_idx].clk_sig = sig;
}

// Start a clock with a null phase
void ClockGen::StartClock(int clk_idx, vluint64_t stamp_ps)
{
    StartClock(clk_idx, 0, stamp_ps);
}

// Start a clock with a phase
void ClockGen::StartClock(int clk_idx, vluint64_t phase_ps, vluint64_t stamp_ps)
{
    // Boundary check
    if (clk_idx < num_clock)
    {
        // Clock pointer
        vlclk_t *p = clk_map + clk_idx;
        // Start with a 0
        p->clk_state = (vluint8_t)0;
        *p->clk_sig  = (vluint8_t)0;
        // Check if the half period is not null
        if (p->clk_hper_ps)
        {
            // Compute where we are in the clock's period
            vluint64_t rem = stamp_ps % (p->clk_hper_ps << 1);
            // Next rising edge : phase shift + one half period later
            p->clk_stamp_ps = stamp_ps - rem + phase_ps + p->clk_hper_ps;
            // To prevent going back in time !!!
            if (rem >= (phase_ps + p->clk_hper_ps))
            {
                p->clk_stamp_ps += (p->clk_hper_ps << 1);
            }
            // Re-adjust the next stamp
            if (next_stamp_ps > p->clk_stamp_ps)
            {
                next_stamp_ps = p->clk_stamp_ps;
            }
            // Enable the clock
            p->clk_enable = true;
            // Debug message
            printf("\nStartClock(%d) : time = %lld, phase = %lld, stamp = %lld\n",
                   clk_idx, stamp_ps, phase_ps, p->clk_stamp_ps);
        }
    }
}

// Stop a clock
void ClockGen::StopClock(int clk_idx)
{
    // Boundary check
    if (clk_idx >= num_clock) return;
    // Disable the clock
    clk_map[clk_idx].clk_enable = false;
}

// Undivided clock, phase can be 0 (0 deg) or 1 (180 deg)
vluint8_t ClockGen::GetClockStateDiv1(int clk_idx, vluint8_t phase)
{
    // Boundary check
    if (clk_idx >= num_clock) return (vluint8_t)0;
    // Return clock's state
    return (clk_map[clk_idx].clk_state - phase) & 1;
}

// Clock divided by 2, phase can be 0 (0 deg), 1 (90 deg), 2 (180 deg) or 3 (270 deg)
vluint8_t ClockGen::GetClockStateDiv2(int clk_idx, vluint8_t phase)
{
    // Boundary check
    if (clk_idx >= num_clock) return (vluint8_t)0;
    // Return clock's state
    return ((clk_map[clk_idx].clk_state - phase) >> 1) & 1;
}

// Clock divided by 4, phase can be 0 (0 deg) - 7 (315 deg)
vluint8_t ClockGen::GetClockStateDiv4(int clk_idx, vluint8_t phase)
{
    // Boundary check
    if (clk_idx >= num_clock) return (vluint8_t)0;
    // Return clock's state
    return ((clk_map[clk_idx].clk_state - phase) >> 2) & 1;
}

// Clock divided by 8, phase can be 0 (0 deg) - 15 (337.5 deg)
vluint8_t ClockGen::GetClockStateDiv8(int clk_idx, vluint8_t phase)
{
    // Boundary check
    if (clk_idx >= num_clock) return (vluint8_t)0;
    // Return clock's state
    return ((clk_map[clk_idx].clk_state - phase) >> 3) & 1;
}

// Clock divided by 16, phase can be 0 (0 deg) - 31 (348.75 deg)
vluint8_t ClockGen::GetClockStateDiv16(int clk_idx, vluint8_t phase)
{
    // Boundary check
    if (clk_idx >= num_clock) return (vluint8_t)0;
    // Return clock's state
    return ((clk_map[clk_idx].clk_state - phase) >> 4) & 1;
}

// Clock divided by 32, phase can be 0 (0 deg) - 63 (354.375 deg)
vluint8_t ClockGen::GetClockStateDiv32(int clk_idx, vluint8_t phase)
{
    // Boundary check
    if (clk_idx >= num_clock) return (vluint8_t)0;
    // Return clock's state
    return ((clk_map[clk_idx].clk_state - phase) >> 5) & 1;
}

// Update clock states, compute next time stamp
void ClockGen::AdvanceClocks(vluint64_t &stamp_ps, bool quiet)
{
    // Check if an event must be trigerred
    if ((evt_ptr->evt_cback) && (evt_ptr->evt_stamp_ps <= stamp_ps))
    {
        // Call the function
        evt_ptr->evt_cback();
        // Clear the event
        evt_ptr->evt_stamp_ps = (vluint64_t)-1;
        evt_ptr->evt_cback    = NULL;
        // Next event (if present)
        if (--event_stored)
        {
            if ((++event_rd_idx) == event_depth) event_rd_idx = 0;
            evt_ptr = &evt_buf[event_rd_idx];
        }
    }
    
    stamp_ps       = next_stamp_ps;
    next_stamp_ps += max_step_ps;
    
    vlclk_t *p = clk_map;
    for (int i = 0; i < num_clock; i++)
    {
        if (p->clk_enable)
        {
            // Update clock state
            if (p->clk_stamp_ps == stamp_ps)
            {
                p->clk_stamp_ps += p->clk_hper_ps;
                p->clk_state++;
                // Update connected signal
                *p->clk_sig = p->clk_state & 1;
            }
            // Find next time stamp
            if (p->clk_stamp_ps < next_stamp_ps)
            {
                next_stamp_ps = p->clk_stamp_ps;
            }
        }
        p++;
    }
    
    // Quiet mode : no progress
    if (quiet) return;
    
    // Show progress, in microseconds
    if (!(vluint16_t)stamp_ps)
    {
        printf("\r%lld us", stamp_ps / 1000000 );
        fflush(stdout);
    }
}
