#ifndef PIPELINE_STRUCTS_HPP
#define PIPELINE_STRUCTS_HPP

#include <cstdint>

// IF/ID Latch
struct IF_ID {
    uint32_t IR;      // Instruction Register
    uint32_t NPC;     // Next PC (PC + 4)
    uint32_t PC;      // Current PC (for display)
};

// ID/EX Latch
struct ID_EX {
    uint32_t IR;
    uint32_t NPC;
    uint32_t A;       // rs1 value
    uint32_t B;       // rs2 value
    int32_t  IMM;     // Immediate (Sign Extended)
    
    // Control Signals
    uint8_t  func3;
    uint8_t  func7;
    uint8_t  opcode;
    uint8_t  rd;
    uint8_t  rs1;     // Needed for Hazard Detection in EX (if forwarding, but we keep for debug)
    uint8_t  rs2;
    
    // Control Flags
    bool RegWrite;
    bool MemRead;
    bool MemWrite;
    bool Branch;      // BEQ, BLT
    uint8_t ALUOp;    // Custom codes for ALU control
};

// EX/MEM Latch
struct EX_MEM {
    uint32_t IR;
    int32_t  ALUOutput;
    uint32_t B;       // Value to store (SW)
    bool     cond;    // ALU condition (Zero/Less Than)
    
    // Pass-through Controls
    uint8_t  rd;
    bool     RegWrite;
    bool     MemRead;
    bool     MemWrite;
    bool     Branch;
};

// MEM/WB Latch
struct MEM_WB {
    uint32_t IR;
    int32_t  ALUOutput;
    int32_t  LMD;     // Load Memory Data
    
    // Pass-through Controls
    uint8_t  rd;
    bool     RegWrite;
};

#endif