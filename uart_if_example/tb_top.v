
// Trace configuration
// -------------------
`verilator_config

tracing_on -file "./uart_delay.v"

`verilog

`include "./uart_delay.v"
