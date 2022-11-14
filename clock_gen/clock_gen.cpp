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

#include "verilated.h"
#include "clock_gen.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// Constructor : set the number of clocks
ClockGen::ClockGen(int num_clk) :
    m_maxStep_ps    { (vluint64_t)0 },
    m_nextStamp_ps  { (vluint64_t)0 },
    m_event         { (vluint64_t)-1, NULL },
    m_clockMax      { num_clk }
{
    // Allocate the clocks
    m_clockList.resize(num_clk);
    
    // Clear the clocks
    for (auto p = m_clockList.begin(); p != m_clockList.end(); ++p)
    {
        p->clk_stamp_ps = (vluint64_t)0;
        p->clk_sig      = &p->clk_dummy;
        p->clk_hper_ps  = (vluint32_t)0;
        p->clk_state    = (vluint8_t)0;
        p->clk_dummy    = (vluint8_t)0;
        p->clk_enable   = false;
    }
}

// Destructor
ClockGen::~ClockGen()
{
    // Clear the event list
    while (!m_eventList.empty()) m_eventList.pop();
    // Clear the clock list
    m_clockList.clear();
}

// Add an event
void ClockGen::AddEvent(vluint64_t stamp_ps, void (*cback)())
{
    vl_evt_t tmp = { stamp_ps, cback };
    
    // Push a new event to the event list
    m_eventList.push(tmp);
    
    // Keep the closest event
    if (stamp_ps < m_event.evt_stamp_ps)
    {
        m_event = tmp;
    }
}

// Create a new clock
void ClockGen::NewClock(int idx, vluint64_t period_ps)
{
    // Boundary check
    if (idx >= m_clockMax) return;
    // Store the clock's half period
    m_clockList[idx].clk_hper_ps = (vluint32_t)(period_ps >> 1);
    // Adjust the maximum simulation step
    if (m_maxStep_ps < (period_ps >> 1))
    {
        m_maxStep_ps = (period_ps >> 1) + 1;
    }
}

// Connect the undivided clock to a signal
void ClockGen::ConnectClock(int idx, vluint8_t *sig)
{
    // Boundary check
    if (idx >= m_clockMax) return;
    // Store the signal's memory address
    m_clockList[idx].clk_sig = sig;
}

// Start a clock with a null phase
void ClockGen::StartClock(int idx, vluint64_t stamp_ps)
{
    StartClock(idx, 0, stamp_ps);
}

// Start a clock with a phase
void ClockGen::StartClock(int idx, vluint64_t phase_ps, vluint64_t stamp_ps)
{
    // Boundary check
    if (idx < m_clockMax)
    {
        // Clock pointer
        vl_clk_t *p = &m_clockList[idx];
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
            if (m_nextStamp_ps > p->clk_stamp_ps)
            {
                m_nextStamp_ps = p->clk_stamp_ps;
            }
            // Enable the clock
            p->clk_enable = true;
            // Debug message
            printf("\nStartClock(%d) : time = %ld, phase = %ld, stamp = %ld\n",
                   idx, stamp_ps, phase_ps, p->clk_stamp_ps);
        }
    }
}

// Stop a clock
void ClockGen::StopClock(int idx)
{
    // Boundary check
    if (idx >= m_clockMax) return;
    // Disable the clock
    m_clockList[idx].clk_enable = false;
}

// Undivided clock, phase can be 0 (0 deg) or 1 (180 deg)
vluint8_t ClockGen::GetClockStateDiv1(int idx, vluint8_t phase)
{
    // Boundary check
    if (idx >= m_clockMax) return (vluint8_t)0;
    // Return clock's state
    return (m_clockList[idx].clk_state - phase) & 1;
}

// Clock divided by 2, phase can be 0 (0 deg), 1 (90 deg), 2 (180 deg) or 3 (270 deg)
vluint8_t ClockGen::GetClockStateDiv2(int idx, vluint8_t phase)
{
    // Boundary check
    if (idx >= m_clockMax) return (vluint8_t)0;
    // Return clock's state
    return ((m_clockList[idx].clk_state - phase) >> 1) & 1;
}

// Clock divided by 4, phase can be 0 (0 deg) - 7 (315 deg)
vluint8_t ClockGen::GetClockStateDiv4(int idx, vluint8_t phase)
{
    // Boundary check
    if (idx >= m_clockMax) return (vluint8_t)0;
    // Return clock's state
    return ((m_clockList[idx].clk_state - phase) >> 2) & 1;
}

// Clock divided by 8, phase can be 0 (0 deg) - 15 (337.5 deg)
vluint8_t ClockGen::GetClockStateDiv8(int idx, vluint8_t phase)
{
    // Boundary check
    if (idx >= m_clockMax) return (vluint8_t)0;
    // Return clock's state
    return ((m_clockList[idx].clk_state - phase) >> 3) & 1;
}

// Clock divided by 16, phase can be 0 (0 deg) - 31 (348.75 deg)
vluint8_t ClockGen::GetClockStateDiv16(int idx, vluint8_t phase)
{
    // Boundary check
    if (idx >= m_clockMax) return (vluint8_t)0;
    // Return clock's state
    return ((m_clockList[idx].clk_state - phase) >> 4) & 1;
}

// Clock divided by 32, phase can be 0 (0 deg) - 63 (354.375 deg)
vluint8_t ClockGen::GetClockStateDiv32(int idx, vluint8_t phase)
{
    // Boundary check
    if (idx >= m_clockMax) return (vluint8_t)0;
    // Return clock's state
    return ((m_clockList[idx].clk_state - phase) >> 5) & 1;
}

// Update clock states, compute next time stamp
void ClockGen::AdvanceClocks(vluint64_t &stamp_ps, bool quiet)
{
    // Check if an event must be trigerred
    if (m_event.evt_stamp_ps <= m_nextStamp_ps)
    {
        bool no_edge;
        
        // Event occuring on a clock edge ?
        if (m_event.evt_stamp_ps == m_nextStamp_ps)
        {
            no_edge = false;
        }
        else
        {
            no_edge = true;
            stamp_ps = m_event.evt_stamp_ps;
        }
        // Remove the event from the list
        m_eventList.pop();
        // Call the function
        m_event.evt_cback();
        // Is event list empty ?
        if (m_eventList.empty())
        {
            // No more event
            m_event = { (vluint64_t)-1, NULL };
        }
        else
        {
            // Get the top event
            m_event = m_eventList.top();
        }
        // Skip clock edge evaluate
        if (no_edge) return;
    }
    // New time stamp
    stamp_ps = m_nextStamp_ps;
    
    // Update clocks and find next time stamp
    m_nextStamp_ps += m_maxStep_ps;
    for (auto p = m_clockList.begin(); p != m_clockList.end(); ++p)
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
            if (p->clk_stamp_ps < m_nextStamp_ps)
            {
                m_nextStamp_ps = p->clk_stamp_ps;
            }
        }
    }
    
    // Quiet mode : no progress
    if (quiet) return;
    
    // Show progress, in microseconds
    if (!(vluint16_t)stamp_ps)
    {
        printf("%ld us\r", stamp_ps / 1000000 );
        fflush(stdout);
    }
}
