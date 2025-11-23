#include "simulator.hpp"
#include <iostream>
#include <cstring>

// Opcode Constants
#define OP_R_TYPE 0x33
#define OP_I_TYPE 0x13
#define OP_LW     0x03
#define OP_SW     0x23
#define OP_BRANCH 0x63

RISCV_Simulator::RISCV_Simulator(std::map<unsigned int, unsigned int>& imem) 
    : inst_memory(imem) 
{
    std::memset(registers, 0, sizeof(registers));
    std::memset(data_memory, 0, sizeof(data_memory));
    pc = INSTRUCTION_MEMORY_START; 
    cycle = 0;
    stall_pipeline = false;
    
    std::memset(&if_id, 0, sizeof(if_id));
    std::memset(&id_ex, 0, sizeof(id_ex));
    std::memset(&ex_mem, 0, sizeof(ex_mem));
    std::memset(&mem_wb, 0, sizeof(mem_wb));
    
    if_id_next = if_id;
    id_ex_next = id_ex;
    ex_mem_next = ex_mem;
    mem_wb_next = mem_wb;
}

int32_t RISCV_Simulator::sign_extend(uint32_t inst, int type) {
    int32_t value = 0;
    if (type == 0) { // I-type
        value = (inst >> 20);
        if (value & 0x800) value |= 0xFFFFF000;
    } else if (type == 1) { // S-type
        value = ((inst >> 25) << 5) | ((inst >> 7) & 0x1F);
        if (value & 0x800) value |= 0xFFFFF000;
    } else if (type == 2) { // B-type
        value = ((inst >> 31) << 12) | ((inst & 0x7E000000) >> 20) | ((inst & 0xF00) >> 8) | ((inst & 0x80) >> 7); 
        value = (value << 19) >> 19;
    }
    return value;
}

void RISCV_Simulator::step() {
    cycle++;

    // =================================================================
    // 1. WRITE BACK (WB) STAGE
    // =================================================================
    if (mem_wb.RegWrite && mem_wb.rd != 0) {
        int32_t data = (mem_wb.IR & 0x7F) == OP_LW ? mem_wb.LMD : mem_wb.ALUOutput;
        registers[mem_wb.rd] = data;
        registers[0] = 0; // Hardwire x0
        
        std::cout << "[DEBUG-WB] Wrote value " << data << " (0x" << std::hex << data << std::dec << ") to Register x" << (int)mem_wb.rd << "\n";
    }

    // =================================================================
    // 2. MEMORY (MEM) STAGE (FIXED FOR 32-BIT R/W)
    // =================================================================
    mem_wb_next.IR = ex_mem.IR;
    mem_wb_next.ALUOutput = ex_mem.ALUOutput;
    mem_wb_next.rd = ex_mem.rd;
    mem_wb_next.RegWrite = ex_mem.RegWrite;
    mem_wb_next.LMD = 0;

    // HANDLE LOAD WORD (Read 4 Bytes)
    if (ex_mem.MemRead) { 
        if (ex_mem.ALUOutput >= 0 && ex_mem.ALUOutput <= 124) { // Bounds check (128 - 4)
            uint32_t b0 = data_memory[ex_mem.ALUOutput];
            uint32_t b1 = data_memory[ex_mem.ALUOutput + 1];
            uint32_t b2 = data_memory[ex_mem.ALUOutput + 2];
            uint32_t b3 = data_memory[ex_mem.ALUOutput + 3];
            
            // Reconstruct 32-bit integer (Little Endian)
            mem_wb_next.LMD = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
            
            std::cout << "[DEBUG-MEM] LW read " << mem_wb_next.LMD << " from Address " << ex_mem.ALUOutput << "\n";
        } else {
            std::cout << "[DEBUG-MEM] LW Error: Address " << ex_mem.ALUOutput << " out of bounds\n";
        }
    }
    
    // HANDLE STORE WORD (Write 4 Bytes)
    if (ex_mem.MemWrite) { 
        if (ex_mem.ALUOutput >= 0 && ex_mem.ALUOutput <= 124) {
            uint32_t val = ex_mem.B;
            
            // Break 32-bit int into 4 bytes
            data_memory[ex_mem.ALUOutput]     = val & 0xFF;
            data_memory[ex_mem.ALUOutput + 1] = (val >> 8) & 0xFF;
            data_memory[ex_mem.ALUOutput + 2] = (val >> 16) & 0xFF;
            data_memory[ex_mem.ALUOutput + 3] = (val >> 24) & 0xFF;
            
            std::cout << "[DEBUG-MEM] SW wrote " << val << " to Address " << ex_mem.ALUOutput << " (4 bytes)\n";
        }
    }

    // =================================================================
    // 3. EXECUTE (EX) STAGE
    // =================================================================
    ex_mem_next.IR = id_ex.IR;
    ex_mem_next.B = id_ex.B;
    ex_mem_next.rd = id_ex.rd;
    ex_mem_next.RegWrite = id_ex.RegWrite;
    ex_mem_next.MemRead = id_ex.MemRead;
    ex_mem_next.MemWrite = id_ex.MemWrite;
    ex_mem_next.Branch = id_ex.Branch;
    ex_mem_next.cond = false;
    ex_mem_next.ALUOutput = 0;

    if (id_ex.IR != 0) {
        int32_t op1 = id_ex.A;
        int32_t op2 = (id_ex.opcode == OP_I_TYPE || id_ex.opcode == OP_LW || id_ex.opcode == OP_SW) ? id_ex.IMM : id_ex.B;
        
        if (id_ex.opcode == OP_R_TYPE) {
            if (id_ex.func3 == 0x0) { // ADD, SUB
                if (id_ex.func7 == 0x20) ex_mem_next.ALUOutput = op1 - op2;
                else ex_mem_next.ALUOutput = op1 + op2;
            }
            else if (id_ex.func3 == 0x1) ex_mem_next.ALUOutput = op1 << op2; // SLL
            else if (id_ex.func3 == 0x2) ex_mem_next.ALUOutput = (op1 < op2) ? 1 : 0; // SLT
        } 
        else if (id_ex.opcode == OP_I_TYPE) {
             if (id_ex.func3 == 0x0) ex_mem_next.ALUOutput = op1 + op2; // ADDI
             else if (id_ex.func3 == 0x1) ex_mem_next.ALUOutput = op1 << (op2 & 0x1F); // SLLI
        }
        else if (id_ex.opcode == OP_LW || id_ex.opcode == OP_SW) {
            ex_mem_next.ALUOutput = op1 + op2; 
        }
        else if (id_ex.opcode == OP_BRANCH) {
            if (id_ex.func3 == 0x0) ex_mem_next.cond = (op1 == op2); // BEQ
            else if (id_ex.func3 == 0x4) ex_mem_next.cond = (op1 < op2); // BLT
        }
    }

    // Control Hazard: Branch Taken
    bool branch_taken = ex_mem_next.Branch && ex_mem_next.cond;
    if (branch_taken) {
        pc = id_ex.NPC + (id_ex.IMM << 1) - 4; 
        std::memset(&if_id_next, 0, sizeof(if_id_next));
        std::memset(&id_ex_next, 0, sizeof(id_ex_next));
        stall_pipeline = true; 
        std::cout << "[DEBUG-EX] Branch Taken! New PC: " << std::hex << pc + 4 << std::dec << "\n";
    }

    // =================================================================
    // 4. DECODE (ID) STAGE
    // =================================================================
    id_ex_next.IR = if_id.IR;
    id_ex_next.NPC = if_id.NPC;
    
    if (if_id.IR != 0 && !stall_pipeline) {
        uint32_t inst = if_id.IR;
        id_ex_next.opcode = inst & 0x7F;
        id_ex_next.rd = (inst >> 7) & 0x1F;
        id_ex_next.func3 = (inst >> 12) & 0x07;
        uint8_t rs1 = (inst >> 15) & 0x1F;
        uint8_t rs2 = (inst >> 20) & 0x1F;
        id_ex_next.func7 = (inst >> 25) & 0x7F;

        id_ex_next.A = registers[rs1];
        id_ex_next.B = registers[rs2];
        id_ex_next.rs1 = rs1;
        id_ex_next.rs2 = rs2;

        id_ex_next.RegWrite = (id_ex_next.opcode == OP_R_TYPE || id_ex_next.opcode == OP_I_TYPE || id_ex_next.opcode == OP_LW);
        id_ex_next.MemRead  = (id_ex_next.opcode == OP_LW);
        id_ex_next.MemWrite = (id_ex_next.opcode == OP_SW);
        id_ex_next.Branch   = (id_ex_next.opcode == OP_BRANCH);

        if (id_ex_next.opcode == OP_I_TYPE || id_ex_next.opcode == OP_LW) id_ex_next.IMM = sign_extend(inst, 0);
        else if (id_ex_next.opcode == OP_SW) id_ex_next.IMM = sign_extend(inst, 1);
        else if (id_ex_next.opcode == OP_BRANCH) id_ex_next.IMM = sign_extend(inst, 2);
        else id_ex_next.IMM = 0;

        // Hazard Detection (RAW)
        bool hazard = false;
        if (id_ex.RegWrite && id_ex.rd != 0 && (id_ex.rd == rs1 || id_ex.rd == rs2)) hazard = true;
        if (ex_mem.RegWrite && ex_mem.rd != 0 && (ex_mem.rd == rs1 || ex_mem.rd == rs2)) hazard = true;
        if (mem_wb.RegWrite && mem_wb.rd != 0 && (mem_wb.rd == rs1 || mem_wb.rd == rs2)) hazard = true;

        if (hazard) {
            std::memset(&id_ex_next, 0, sizeof(id_ex_next));
            if_id_next = if_id; 
            stall_pipeline = true;
        }
    }

    // =================================================================
    // 5. FETCH (IF) STAGE
    // =================================================================
    if (!stall_pipeline) {
        if (inst_memory.count(pc)) {
            if_id_next.IR = inst_memory[pc];
            if_id_next.PC = pc;
            if_id_next.NPC = pc + 4;
            pc += 4;
        } else {
            if_id_next.IR = 0; 
        }
    } else {
        stall_pipeline = false;
    }

    mem_wb = mem_wb_next;
    ex_mem = ex_mem_next;
    id_ex  = id_ex_next;
    if_id  = if_id_next;
}