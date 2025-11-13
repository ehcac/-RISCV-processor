#include "assembler.hpp"
#include "parser.hpp"
#include "encoder.hpp"

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

    return 0;
}
