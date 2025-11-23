#include "assembler.hpp"
#include "parser.hpp"
#include "encoder.hpp"
#include "simulator.hpp" 
#include <limits>       

void clear_screen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

void print_pipeline_state(RISCV_Simulator& sim) {
    IF_ID if_id = sim.get_if_id();
    ID_EX id_ex = sim.get_id_ex();
    EX_MEM ex_mem = sim.get_ex_mem();
    MEM_WB mem_wb = sim.get_mem_wb();

    cout << "\n================ PIPELINE STATE MAP ================\n";
    cout << "[IF/ID] PC: " << hex << if_id.PC << " | IR: " << if_id.IR << " | NPC: " << if_id.NPC << dec << endl;
    cout << "[ID/EX] IR: " << hex << id_ex.IR << " | A: " << dec << id_ex.A << " | B: " << id_ex.B << " | IMM: " << id_ex.IMM << " | NPC: " << hex << id_ex.NPC << endl;
    cout << "[EX/MEM] IR: " << hex << ex_mem.IR << " | ALUOutput: " << dec << ex_mem.ALUOutput << " | B: " << ex_mem.B << " | Cond: " << (ex_mem.cond ? "True" : "False") << endl;
    cout << "[MEM/WB] IR: " << hex << mem_wb.IR << " | ALUOutput: " << dec << mem_wb.ALUOutput << " | LMD: " << mem_wb.LMD << endl;
    
    if (mem_wb.RegWrite && mem_wb.rd != 0) {
        cout << "[WB] Writing to x" << (int)mem_wb.rd << endl;
    }
    cout << "====================================================\n";
}

void print_registers(RISCV_Simulator& sim) {
    cout << "\n--- REGISTER FILE (x0 - x31) ---\n";
    for (int i = 0; i < 32; i+=4) {
        cout << "x" << setfill('0') << setw(2) << i << ": " << setw(8) << hex << sim.get_reg(i) << "\t";
        cout << "x" << setfill('0') << setw(2) << i+1 << ": " << setw(8) << hex << sim.get_reg(i+1) << "\t";
        cout << "x" << setfill('0') << setw(2) << i+2 << ": " << setw(8) << hex << sim.get_reg(i+2) << "\t";
        cout << "x" << setfill('0') << setw(2) << i+3 << ": " << setw(8) << hex << sim.get_reg(i+3) << endl;
    }
    cout << dec; 
}

void menu_edit_memory(RISCV_Simulator& sim) {
    int addr, val;
    cout << "Enter Address (0-127) [DEC]: ";
    cin >> addr;
    cout << "Enter Value (0-255) [DEC]: ";
    cin >> val;
    sim.set_memory(addr, val); 
    cout << "Memory Updated.\n"; 
}

void menu_edit_register(RISCV_Simulator& sim) {
    int idx, val;
    cout << "Enter Register Index (1-31): ";
    cin >> idx;
    cout << "Enter Value: ";
    cin >> val;
    sim.set_reg(idx, val);
    cout << "Register Updated.\n";
}

int main() {
    string filename;
    cout << "Enter the RISC-V file name: ";
    getline(cin, filename);

    vector<string> lines = readAndPreprocess(filename);
    if (lines.empty()) {
        cout << "Input file is empty or missing executable code." << endl;
        return 0;
    }

    SYMBOL_TABLE = buildSymbolTable(lines);
    parseDataSection(lines);
    vector<ParsedInstruction> instructions = parseInstructions(lines);

    cout << "\n--- Opcode Translation ---" << endl;
    INSTRUCTION_MEMORY = translateToOpcode(instructions);

    cout << "Address\t\tOpcode (Hex)\tInstruction" << endl;
    cout << "------------------------------------------------" << endl;
    for (const ParsedInstruction& inst : instructions) {
        unsigned int opcode = INSTRUCTION_MEMORY.at(inst.address);
        cout << "0x" << setw(8) << setfill('0') << hex << uppercase << inst.address
             << "\t0x" << setw(8) << setfill('0') << hex << uppercase << opcode
             << "\t" << inst.originalLine
             << endl;
    }
    cout << dec; 

    cout << "\nInitializing Simulator..." << endl;
    RISCV_Simulator sim(INSTRUCTION_MEMORY);

    if (!DATA_SEGMENT.empty()) {
        cout << "\n--- Loading Data Segment (.data) ---\n";
        for (auto const& [addr, val] : DATA_SEGMENT) {
            sim.set_memory(addr,     val & 0xFF);
            sim.set_memory(addr + 1, (val >> 8) & 0xFF);
            sim.set_memory(addr + 2, (val >> 16) & 0xFF);
            sim.set_memory(addr + 3, (val >> 24) & 0xFF);
            cout << "Loaded .word " << val << " at address " << addr << endl;
        }
    }

    cout << "\n--- PRE-EXECUTION CONFIGURATION ---" << endl;
    bool configuring = true;
    while(configuring) {
        cout << "[1] Set Register Value\n[2] Set Memory Value\n[3] Start Simulation\nChoice: ";
        int choice;
        cin >> choice;
        if (choice == 1) menu_edit_register(sim);
        else if (choice == 2) menu_edit_memory(sim);
        else if (choice == 3) configuring = false;
    }

    bool running = true;
    while(running) {
        clear_screen(); 
        
        cout << "PC: 0x" << hex << sim.get_pc() << dec << "\n";
        print_pipeline_state(sim);
        print_registers(sim);

        cout << "\n--- SIMULATION CONTROLS ---\n";
        cout << "[1] Step (Execute 1 Cycle)\n";
        cout << "[2] Run All (Until End)\n";
        cout << "[3] View/Goto Memory\n";
        cout << "[4] Exit\n";
        cout << "Choice: ";
        
        int choice;
        cin >> choice;

        switch(choice) {
            case 1: 
                sim.step();
                break;
            case 2: 
                for(int i=0; i<1000; i++) { 
                    sim.step();
                    if (sim.get_pc() > instructions.back().address + 4) break;
                }
                break;
            case 3: { // THIS IS THE UPDATED PART FOR 32-BIT VIEWING
                int addr;
                cout << "Enter Memory Address (0-127): ";
                cin >> addr;
                
                // Read raw byte
                uint8_t byteVal = sim.get_mem(addr);
                cout << "Byte at " << addr << ": " << (int)byteVal << " (0x" << hex << (int)byteVal << dec << ")" << endl;
                
                // Read full word (4 bytes)
                if (addr <= 124) {
                    int32_t wordVal = sim.get_mem(addr) | 
                                     (sim.get_mem(addr+1) << 8) | 
                                     (sim.get_mem(addr+2) << 16) | 
                                     (sim.get_mem(addr+3) << 24);
                    cout << "Word at " << addr << " (32-bit): " << wordVal << endl;
                }
                
                cout << "Press Enter to continue...";
                cin.ignore(); cin.get();
                break;
            }
            case 4: 
                running = false; 
                break;
        }
    }

    return 0;
}