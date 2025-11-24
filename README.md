# RISCV-processor
a simulator for a simplified RISC-V processor in C++
## Milestone#1
  - Implemented parsing of RISC-V source code
  - Implemented conversion of RISC-V code to equivalent opcodes (hex)
  - Printed output in console

## To run:
cd cpp_files
emcc *.cpp   -o ../simulator.js   -s WASM=1   -s ALLOW_MEMORY_GROWTH=1   -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]'   -s EXPORT_ES6=0   --bind   -std=c++17   -O2
python3 -m http.server 8000 # OR python -m http.server 8000

## Supported Instructions:
LW, SW, SLT, SLL, SLLI, BEQ, BLT

## Project Structure:
- assembler.hpp - contains shared data structures, constants, and global declarations
- instruction_set.cpp - contains the RISC-V instruction definitions
- utils.hpp / utils.cpp - for helper/utility functions (e.g., splitting, conversions, register parsing)
- parser.hpp / parser.cpp - handles reading, and instruction parsing
- encoder.hpp / encoder.cpp - contains functions for translation to opcode
- main.cpp - main program
