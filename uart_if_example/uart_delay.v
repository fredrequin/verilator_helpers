module uart_delay
(
    input  bclk,
    input  uart_rx,
    output uart_tx
);

// ~217 us delay
reg [124:0] r_uart_dly;

initial begin
    r_uart_dly = 125'h1FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF;
end

always @ (posedge bclk) begin
    r_uart_dly <= { r_uart_dly[123:0], uart_rx };
end
    
assign uart_tx = r_uart_dly[124];

endmodule
