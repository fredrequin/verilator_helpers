#! /bin/sh

#Verilog top module
TOP_FILE=uart_delay

#Options for Verilator
VERILATOR_OPT="-cc -no-decoration -output-split 20000 -output-split-ctrace 10000"
#Options for GCC compiler
COMPILE_OPT="-CFLAGS -DVM_PREFIX=V"$TOP_FILE" -CFLAGS -Wno-attributes -CFLAGS -O2"

#Options for C++ model analysis
#ANALYSIS_OPT="-stats -Wwarn-IMPERFECTSCH"

#Comment this line to disable VCD generation
TRACE_OPT="-trace -no-trace-params"

#Clock signals
CLOCK_OPT=\
"-clk v.clk"

#C++ support files
CPP_FILES=\
"main.cpp\
 ../clock_gen/clock_gen.cpp\
 ../uart_if/uart_if.cpp\
 verilated_dpi.cpp"

verilator tb_top.v $ANALYSIS_OPT $VERILATOR_OPT $COMPILE_OPT $CLOCK_OPT $TRACE_OPT -top-module $TOP_FILE -exe $CPP_FILES
cd ./obj_dir
#make CXX=clang OBJCACHE=ccache -j -f V$TOP_FILE.mk V$TOP_FILE
make -j -f V$TOP_FILE.mk V$TOP_FILE
cd ..
