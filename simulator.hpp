#ifndef SIMULATOR_HPP
#define SIMULATOR_HPP

#include "assembler.hpp"
#include "pipeline_structs.hpp"
#include <map>
#include <cstring>

class RISCV_Simulator {
private:
    // --- Architectural State ---
    int32_t registers[32];
    uint8_t data_memory[128]; // 0x00-0x7F
    
    // Reference to Instruction Memory (From your assembler)
    std::map<unsigned int, unsigned int>& inst_memory;
    
    uint32_t pc;
    uint64_t cycle;
    bool stall_pipeline; // Global stall flag

    // --- Pipeline Registers (Double Buffered) ---
    IF_ID  if_id,  if_id_next;
    ID_EX  id_ex,  id_ex_next;
    EX_MEM ex_mem, ex_mem_next;
    MEM_WB mem_wb, mem_wb_next;

    // Internal Helpers
    int32_t sign_extend(uint32_t inst, int type); // 0=I, 1=S, 2=B, 3=J

public:
    RISCV_Simulator(std::map<unsigned int, unsigned int>& imem);

    // Core Execution
    void step();     // Execute 1 Cycle
    void run();      // Run until end
    
    // Getters for GUI/Console Output
    uint32_t get_pc() const { return pc; }
    int32_t get_reg(int idx) const { return registers[idx]; }
    uint8_t get_mem(int addr) const { return data_memory[addr]; }

    void set_reg(int idx, int32_t val) {
        if (idx > 0 && idx < 32) registers[idx] = val;
    }

    void set_memory(int addr, uint8_t val) {
        if (addr >= 0 && addr < 128) data_memory[addr] = val;
    }
    
    // Access to internal pipeline state for display
    IF_ID  get_if_id()  { return if_id; }
    ID_EX  get_id_ex()  { return id_ex; }
    EX_MEM get_ex_mem() { return ex_mem; }
    MEM_WB get_mem_wb() { return mem_wb; }
};

#endif