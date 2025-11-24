# RISCV-processor
a simulator for a simplified RISC-V processor in C++

## Supported Instructions:
LW, SW, SLT, SLL, SLLI, BEQ, BLT
additional instructions: ADD, SUB, AND, ADDI

## Design Methodology:
assembler.hpp - contains shared data structures, constants, and global declarations
encoder.cpp - contains functions for translation to opcode
encoder.hpp - contains functions for translation to opcode
index.html - main webpage used for GUI
instruction_set.cpp - contains the RISC-V instruction definitions
main.cpp - main file containing simulator functions for HTML
parser.cpp - handles reading, and instruction parsing
parser.hpp - handles reading, and instruction parsing
pipeline_structs.hpp - contains data structures used for pipelining
simulator.cpp - contains functions used for simulator in main
simulator.hpp - contains functions used for other larger functions in simulator.cpp
simulator.js - JavaScript file used for index.html
simulator.wasm - wasm file used for index.html
utils.cpp - for helper/utility functions (e.g., splitting, conversions, register parsing)
utils.hpp - for helper/utility functions (e.g., splitting, conversions, register parsing)
