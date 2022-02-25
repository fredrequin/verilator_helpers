#include "Vuart_delay.h"
#include "verilated.h"
#include "../clock_gen/clock_gen.h"
#include "../uart_if/uart_if.h"

#include <ctime>

#if VM_TRACE
#include "verilated_vcd_c.h"
#endif

// Clocks generation (global)
ClockGen *clk;
// UART interface (global)
UartIF *ser;

static void SendMsg_CBack(void)
{
    // Send "Hello world"
    ser->PutTxString("Hello world!\n");
}

int main(int argc, char **argv, char **env)
{
    // Simulation duration
    clock_t beg, end;
    double secs;
    // Trace index
    int trc_idx = 0;
    int min_idx = 0;
    // File name generation
    char file_name[256];
    // Simulation time
    vluint64_t tb_time;
    vluint64_t max_time;
    // Testbench configuration
    const char *arg;
    // UART character
    vluint16_t ch;
    
    beg = clock();
    
    // Parse parameters
    Verilated::commandArgs(argc, argv);
    
    // Default : 1 msec
    max_time = (vluint64_t)1000000000;
    
    // Simulation duration : +usec=<num>
    arg = Verilated::commandArgsPlusMatch("usec=");
    if ((arg) && (arg[0]))
    {
        arg += 6;
        max_time = (vluint64_t)atoi(arg) * (vluint64_t)1000000;
    }
    
    // Simulation duration : +msec=<num>
    arg = Verilated::commandArgsPlusMatch("msec=");
    if ((arg) && (arg[0]))
    {
        arg += 6;
        max_time = (vluint64_t)atoi(arg) * (vluint64_t)1000000000;
    }
    
    // Trace start index : +tidx=<num>
    arg = Verilated::commandArgsPlusMatch("tidx=");
    if ((arg) && (arg[0]))
    {
        arg += 6;
        min_idx = atoi(arg);
    }
    else
    {
        min_idx = 0;
    }
    
    // Initialize top verilog instance
    Vuart_delay* top = new Vuart_delay;
    
    // Initialize clock generator    
    clk = new ClockGen(1, 256);
    tb_time = (vluint64_t)0;
    // Initialize UART interface
    ser = new UartIF();
    ser->ConnectTx(&top->uart_rx);
    ser->ConnectRx(&top->uart_tx);
    // UART clock : 576 KHz (5 x 115200)
    clk->NewClock(0, ser->SetUartConfig("8N1", 115200, 0));
    clk->ConnectClock(0, &top->bclk);
    clk->StartClock(0, tb_time);
    
    // Message sent after 10 us
    clk->AddEvent(TS_US(10), SendMsg_CBack);
    
    
#if VM_TRACE
    // Initialize VCD trace dump
    Verilated::traceEverOn(true);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    top->trace (tfp, 99);
    tfp->spTrace()->set_time_resolution ("1 ps");
    if (trc_idx == min_idx)
    {
        sprintf(file_name, "uart_%04d.vcd", trc_idx);
        printf("Opening VCD file \"%s\"\n", file_name);
        tfp->open (file_name);
    }
#endif /* VM_TRACE */
  
    // Simulation loop
    while (tb_time < max_time)
    {
        // Toggle clocks
        clk->AdvanceClocks(tb_time, true);
        // Evaluate verilated model
        top->eval ();
        
        // Evaluate UART communication
        ser->Eval(clk->GetClockStateDiv1(0,0));
        
        // Display delayed loop-back
        if (ser->GetRxChar(ch) >= RX_OK)
        {
            printf("%c", ch);
        }
        
#if VM_TRACE
        // Dump signals into VCD file
        if (tfp)
        {
            if (trc_idx >= min_idx)
            {
                tfp->dump (tb_time);
            }
        }
#endif /* VM_TRACE */

        if (Verilated::gotFinish()) break;
    }
    
#if VM_TRACE
    if (tfp && trc_idx >= min_idx) tfp->close();
#endif /* VM_TRACE */

    top->final();
    
    delete top;
    
    delete clk;
    
    delete ser;
  
    // Calculate running time
    end = clock();
    printf("\nSeconds elapsed : %5.3f\n", (float)(end - beg) / CLOCKS_PER_SEC);

    exit(0);
}
