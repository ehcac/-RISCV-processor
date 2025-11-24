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
    
    std::cout << "\n========== CYCLE " << cycle << " ==========\n";

    // =================================================================
    // 1. WRITE BACK (WB) STAGE
    // =================================================================
    if (mem_wb.RegWrite && mem_wb.rd != 0) {
        int32_t data = (mem_wb.IR & 0x7F) == OP_LW ? mem_wb.LMD : mem_wb.ALUOutput;
        registers[mem_wb.rd] = data;
        registers[0] = 0; // Hardwire x0
        
        std::cout << "[WB] Wrote " << data << " to x" << (int)mem_wb.rd << "\n";
    } else if (mem_wb.IR != 0) {
        std::cout << "[WB] No write back (NOP or x0)\n";
    }

    // =================================================================
    // 2. MEMORY (MEM) STAGE
    // =================================================================
    mem_wb_next.IR = ex_mem.IR;
    mem_wb_next.ALUOutput = ex_mem.ALUOutput;
    mem_wb_next.rd = ex_mem.rd;
    mem_wb_next.RegWrite = ex_mem.RegWrite;
    mem_wb_next.LMD = 0;

    if (ex_mem.IR != 0) {
        // HANDLE LOAD WORD (Read 4 Bytes)
        if (ex_mem.MemRead) { 
            if (ex_mem.ALUOutput >= 0 && ex_mem.ALUOutput <= 124) {
                uint32_t b0 = data_memory[ex_mem.ALUOutput];
                uint32_t b1 = data_memory[ex_mem.ALUOutput + 1];
                uint32_t b2 = data_memory[ex_mem.ALUOutput + 2];
                uint32_t b3 = data_memory[ex_mem.ALUOutput + 3];
                
                mem_wb_next.LMD = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
                
                std::cout << "[MEM] LW: Read " << mem_wb_next.LMD << " from addr " << ex_mem.ALUOutput << "\n";
            } else {
                std::cout << "[MEM] LW ERROR: Address " << ex_mem.ALUOutput << " out of bounds\n";
            }
        }
        
        // HANDLE STORE WORD (Write 4 Bytes)
        if (ex_mem.MemWrite) { 
            if (ex_mem.ALUOutput >= 0 && ex_mem.ALUOutput <= 124) {
                uint32_t val = ex_mem.B;
                
                data_memory[ex_mem.ALUOutput]     = val & 0xFF;
                data_memory[ex_mem.ALUOutput + 1] = (val >> 8) & 0xFF;
                data_memory[ex_mem.ALUOutput + 2] = (val >> 16) & 0xFF;
                data_memory[ex_mem.ALUOutput + 3] = (val >> 24) & 0xFF;
                
                std::cout << "[MEM] SW: Wrote " << val << " to addr " << ex_mem.ALUOutput << "\n";
            } else {
                std::cout << "[MEM] SW ERROR: Address " << ex_mem.ALUOutput << " out of bounds\n";
            }
        }
        
        if (!ex_mem.MemRead && !ex_mem.MemWrite) {
            std::cout << "[MEM] No memory operation\n";
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
        
        std::cout << "[EX] Opcode=0x" << std::hex << (int)id_ex.opcode << std::dec;
        
        if (id_ex.opcode == OP_R_TYPE) {
            if (id_ex.func3 == 0x0) { // ADD, SUB
                if (id_ex.func7 == 0x20) {
                    ex_mem_next.ALUOutput = op1 - op2;
                    std::cout << " SUB: " << op1 << " - " << op2 << " = " << ex_mem_next.ALUOutput << "\n";
                } else {
                    ex_mem_next.ALUOutput = op1 + op2;
                    std::cout << " ADD: " << op1 << " + " << op2 << " = " << ex_mem_next.ALUOutput << "\n";
                }
            }
            else if (id_ex.func3 == 0x1) {
                ex_mem_next.ALUOutput = op1 << (op2 & 0x1F);
                std::cout << " SLL: " << op1 << " << " << (op2 & 0x1F) << " = " << ex_mem_next.ALUOutput << "\n";
            }
            else if (id_ex.func3 == 0x2) {
                ex_mem_next.ALUOutput = (op1 < op2) ? 1 : 0;
                std::cout << " SLT: " << op1 << " < " << op2 << " = " << ex_mem_next.ALUOutput << "\n";
            }
        } 
        else if (id_ex.opcode == OP_I_TYPE) {
             if (id_ex.func3 == 0x0) {
                 ex_mem_next.ALUOutput = op1 + op2;
                 std::cout << " ADDI: " << op1 << " + " << op2 << " = " << ex_mem_next.ALUOutput << "\n";
             }
             else if (id_ex.func3 == 0x1) {
                 ex_mem_next.ALUOutput = op1 << (op2 & 0x1F);
                 std::cout << " SLLI: " << op1 << " << " << (op2 & 0x1F) << " = " << ex_mem_next.ALUOutput << "\n";
             }
        }
        else if (id_ex.opcode == OP_LW || id_ex.opcode == OP_SW) {
            ex_mem_next.ALUOutput = op1 + op2;
            std::cout << " ADDR: " << op1 << " + " << op2 << " = " << ex_mem_next.ALUOutput << "\n";
        }
        else if (id_ex.opcode == OP_BRANCH) {
            if (id_ex.func3 == 0x0) {
                ex_mem_next.cond = (op1 == op2);
                std::cout << " BEQ: " << op1 << " == " << op2 << " ? " << ex_mem_next.cond << "\n";
            }
            else if (id_ex.func3 == 0x4) {
                ex_mem_next.cond = (op1 < op2);
                std::cout << " BLT: " << op1 << " < " << op2 << " ? " << ex_mem_next.cond << "\n";
            }
        }
    }

    // =================================================================
    // CONTROL HAZARD: Pipeline Freeze on Branch Taken
    // =================================================================
    bool branch_taken = ex_mem_next.Branch && ex_mem_next.cond;
    if (branch_taken) {
        // Calculate branch target
        uint32_t branch_target = id_ex.NPC + (id_ex.IMM << 1) - 4;
        pc = branch_target;
        
        std::cout << "[CONTROL HAZARD] Branch taken! Flushing IF/ID and ID/EX. New PC: 0x" 
                  << std::hex << pc << std::dec << "\n";
        
        // Flush the two instructions that were incorrectly fetched
        std::memset(&if_id_next, 0, sizeof(if_id_next));
        std::memset(&id_ex_next, 0, sizeof(id_ex_next));
        stall_pipeline = true; 
    }

    // =================================================================
    // 4. DECODE (ID) STAGE - DATA HAZARD DETECTION (NO FORWARDING)
    // =================================================================
    id_ex_next.IR = if_id.IR;
    id_ex_next.NPC = if_id.NPC;
    
    bool data_hazard_detected = false;
    
    if (if_id.IR != 0 && !stall_pipeline) {
        uint32_t inst = if_id.IR;
        id_ex_next.opcode = inst & 0x7F;
        id_ex_next.rd = (inst >> 7) & 0x1F;
        id_ex_next.func3 = (inst >> 12) & 0x07;
        uint8_t rs1 = (inst >> 15) & 0x1F;
        uint8_t rs2 = (inst >> 20) & 0x1F;
        id_ex_next.func7 = (inst >> 25) & 0x7F;

        id_ex_next.rs1 = rs1;
        id_ex_next.rs2 = rs2;

        // Set control signals
        id_ex_next.RegWrite = (id_ex_next.opcode == OP_R_TYPE || id_ex_next.opcode == OP_I_TYPE || id_ex_next.opcode == OP_LW);
        id_ex_next.MemRead  = (id_ex_next.opcode == OP_LW);
        id_ex_next.MemWrite = (id_ex_next.opcode == OP_SW);
        id_ex_next.Branch   = (id_ex_next.opcode == OP_BRANCH);

        // Sign extend immediate
        if (id_ex_next.opcode == OP_I_TYPE || id_ex_next.opcode == OP_LW) {
            id_ex_next.IMM = sign_extend(inst, 0);
        }
        else if (id_ex_next.opcode == OP_SW) {
            id_ex_next.IMM = sign_extend(inst, 1);
        }
        else if (id_ex_next.opcode == OP_BRANCH) {
            id_ex_next.IMM = sign_extend(inst, 2);
        }
        else {
            id_ex_next.IMM = 0;
        }

        // =================================================================
        // DATA HAZARD DETECTION: NO FORWARDING - Must stall until data is written back
        // =================================================================
        // Check if current instruction needs rs1 or rs2
        bool needs_rs1 = (id_ex_next.opcode == OP_R_TYPE || id_ex_next.opcode == OP_I_TYPE || 
                          id_ex_next.opcode == OP_LW || id_ex_next.opcode == OP_SW || 
                          id_ex_next.opcode == OP_BRANCH);
        bool needs_rs2 = (id_ex_next.opcode == OP_R_TYPE || id_ex_next.opcode == OP_SW || 
                          id_ex_next.opcode == OP_BRANCH);

        std::cout << "[ID] Decoding IR=0x" << std::hex << inst << std::dec 
                  << " rs1=x" << (int)rs1 << " rs2=x" << (int)rs2 << "\n";

        // Check for RAW hazards in EX stage (1 cycle away)
        if (id_ex.RegWrite && id_ex.rd != 0) {
            if ((needs_rs1 && id_ex.rd == rs1) || (needs_rs2 && id_ex.rd == rs2)) {
                data_hazard_detected = true;
                std::cout << "[DATA HAZARD] RAW detected with EX stage (rd=x" << (int)id_ex.rd << ")\n";
            }
        }

        // Check for RAW hazards in MEM stage (2 cycles away)
        if (ex_mem.RegWrite && ex_mem.rd != 0) {
            if ((needs_rs1 && ex_mem.rd == rs1) || (needs_rs2 && ex_mem.rd == rs2)) {
                data_hazard_detected = true;
                std::cout << "[DATA HAZARD] RAW detected with MEM stage (rd=x" << (int)ex_mem.rd << ")\n";
            }
        }

        // Check for RAW hazards in WB stage (3 cycles away)
        if (mem_wb.RegWrite && mem_wb.rd != 0) {
            if ((needs_rs1 && mem_wb.rd == rs1) || (needs_rs2 && mem_wb.rd == rs2)) {
                data_hazard_detected = true;
                std::cout << "[DATA HAZARD] RAW detected with WB stage (rd=x" << (int)mem_wb.rd << ")\n";
            }
        }

        // If hazard detected, insert bubble (NOP) and stall
        if (data_hazard_detected) {
            std::cout << "[STALL] Inserting bubble, keeping IF/ID unchanged\n";
            std::memset(&id_ex_next, 0, sizeof(id_ex_next)); // Insert NOP
            if_id_next = if_id; // Keep IF/ID unchanged
            stall_pipeline = true;
        } else {
            // No hazard, read register values
            id_ex_next.A = registers[rs1];
            id_ex_next.B = registers[rs2];
            std::cout << "[ID] Read A=x" << (int)rs1 << "=" << id_ex_next.A 
                      << ", B=x" << (int)rs2 << "=" << id_ex_next.B << "\n";
        }
    } else if (if_id.IR == 0) {
        std::cout << "[ID] Bubble (NOP)\n";
        std::memset(&id_ex_next, 0, sizeof(id_ex_next));
    }

    // =================================================================
    // 5. FETCH (IF) STAGE
    // =================================================================
    if (!stall_pipeline) {
        if (inst_memory.count(pc)) {
            if_id_next.IR = inst_memory[pc];
            if_id_next.PC = pc;
            if_id_next.NPC = pc + 4;
            std::cout << "[IF] Fetched IR=0x" << std::hex << if_id_next.IR << " from PC=0x" << pc << std::dec << "\n";
            pc += 4;
        } else {
            std::cout << "[IF] No instruction at PC=0x" << std::hex << pc << std::dec << " (End of program)\n";
            if_id_next.IR = 0;
        }
    } else {
        std::cout << "[IF] Pipeline stalled (not fetching)\n";
        stall_pipeline = false; // Reset stall flag for next cycle
    }

    // =================================================================
    // UPDATE PIPELINE REGISTERS
    // =================================================================
    mem_wb = mem_wb_next;
    ex_mem = ex_mem_next;
    id_ex  = id_ex_next;
    if_id  = if_id_next;
    
    std::cout << "========================================\n";
}