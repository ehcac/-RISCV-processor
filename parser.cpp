#include "parser.hpp"

/**
 * Reads the file, ignores comments, and collects non-empty lines.
 */
vector<string> readAndPreprocess(const string& filename) {
    vector<string> lines;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "ERROR: Could not open file " << filename << endl;
        exit(1);
    }

    string line;
    while (getline(file, line)) {
        size_t commentPos = line.find('#');
        if (commentPos != string::npos) {
            line = line.substr(0, commentPos);
        }

        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (!line.empty()) {
            // ignore .word and stuff
            if (line[0] == '.') {
                continue; 
            }
            lines.push_back(line);
        }
    }
    
    return lines;
}

/**
 * Pass 1: build the Symbol Table.
 */
map<string, unsigned int> buildSymbolTable(const vector<string>& lines) {
    map<string, unsigned int> symbolTable;
    unsigned int currentAddress = INSTRUCTION_MEMORY_START;

    for (const string& line : lines) {
        string tempLine = line;

        if (!tempLine.empty() && tempLine[0] == '.') {
            continue;
        }
        
        // Check for a label (ends with ':')
        size_t labelPos = tempLine.find(':');
        if (labelPos != string::npos) {
            // Extract label name (before the ':')
            string label = line.substr(0, labelPos);
            label.erase(remove_if(label.begin(), label.end(), ::isspace), label.end());
            
            if (symbolTable.count(label)) {
                cerr << "ERROR: Duplicate label definition: " << label << endl;
                exit(1);
            }
            symbolTable[label] = currentAddress;
            
            string restOfLine = line.substr(labelPos + 1);
            restOfLine.erase(0, restOfLine.find_first_not_of(" \t\r\n"));
            
            if (!restOfLine.empty()) {
                currentAddress += 4;
            }
        } else {
            currentAddress += 4;
        }
    }
    return symbolTable;
}

// Assembler Phase 2: Parsing, Validation & Encoding Setup

/**
 * Pass 2: Parses instructions and prepares them for encoding.
 * The logic extracts mnemonic and operands, handling the special S/I-Type format 
 * (e.g., lw rd, imm(rs1)) and J-Type alias (j label).
 */
vector<ParsedInstruction> parseInstructions(const vector<string>& lines) {
    vector<ParsedInstruction> instructions;
    unsigned int currentAddress = INSTRUCTION_MEMORY_START;

    for (const string& line : lines) {
        string currentLine = line;
        
        size_t labelPos = currentLine.find(':');
        if (labelPos != string::npos) {
            currentLine = currentLine.substr(labelPos + 1);
            currentLine.erase(0, currentLine.find_first_not_of(" \t\r\n"));
            if (currentLine.empty()) {
                continue; 
            }
        }

        // Split into mnemonic and the rest of the operands
        stringstream ss(currentLine);
        string mnemonic;
        ss >> mnemonic;

        if (mnemonic.empty()) continue;

        string restOfLine;
        getline(ss, restOfLine);

        ParsedInstruction pInst;
        pInst.mnemonic = mnemonic;
        pInst.address = currentAddress;
        pInst.originalLine = line;

        // Handle the special format for loads/stores: lw rd, imm(rs1)
        if (mnemonic == "lw" || mnemonic == "sw") {
            // Split the rest by comma: "rd/rs2, imm(rs1)"
            vector<string> parts = split(restOfLine, ',');
            if (parts.size() != 2) {
                cerr << "ERROR on line: " << line << " -> Incorrect operand count for " << mnemonic << endl;
                exit(1);
            }
            
            string destReg = parts[0]; // rd for lw, rs2 for sw
            string immAndBase = parts[1]; // imm(rs1)

            // Find the opening '(' and closing ')'
            size_t openParen = immAndBase.find('(');
            size_t closeParen = immAndBase.find(')');

            if (openParen == string::npos || closeParen == string::npos || closeParen < openParen) {
                cerr << "ERROR on line: " << line << " -> Invalid address format for " << mnemonic << ". Expected: imm(rs1)" << endl;
                exit(1);
            }

            string imm = immAndBase.substr(0, openParen);
            string baseReg = immAndBase.substr(openParen + 1, closeParen - (openParen + 1));

            baseReg.erase(remove_if(baseReg.begin(), baseReg.end(), ::isspace), baseReg.end());
            
            pInst.operands.push_back(destReg);
            pInst.operands.push_back(baseReg);
            pInst.operands.push_back(imm);

        } else {
            pInst.operands = split(restOfLine, ',');
        }

        instructions.push_back(pInst);
        currentAddress += 4;
    }

    return instructions;
}

/**
 * Part 2: Error-checking phase.
 * Performs Opcode, Register, and Label Resolution checks.
 */
bool validateInstructions(const vector<ParsedInstruction>& instructions) {
    bool success = true;

    for (const ParsedInstruction& inst : instructions) {
        const string& mnemonic = inst.mnemonic;
        const vector<string>& ops = inst.operands;

        // 1. Opcode Validity Check
        if (INSTRUCTION_SET.find(mnemonic) == INSTRUCTION_SET.end()) {
            cerr << "ERROR at address 0x" << hex << inst.address << ": Invalid mnemonic '" << mnemonic << "'." << endl;
            success = false;
            continue;
        }

        const InstructionInfo& info = INSTRUCTION_SET.at(mnemonic);

        // expected operand counts for standard formats
        size_t expectedOps = 0;
        if (info.type == "R") expectedOps = 3; // rd, rs1, rs2
        else if (info.type == "I" && (mnemonic == "lw" || mnemonic == "sw")) expectedOps = 3; // rd, rs1, imm - Handled as special format
        else if (info.type == "I" || info.type == "J") expectedOps = 3; // rd, rs1, imm/label (standard I-Type) OR rd, label (J-Type)
        else if (info.type == "S") expectedOps = 3; // rs2, rs1, imm - Handled as special format
        else if (info.type == "B") expectedOps = 3; // rs1, rs2, label
        
        // Skip count check for the complex load/store format that has 3 elements parsed
        if ((mnemonic != "lw" && mnemonic != "sw") && ops.size() != expectedOps) {
            // Simplified check: R/I/S/B should have 3; J-Type has 2 (rd, label)
            if (info.type == "J" && ops.size() == 2) { 
                // JAL is rd, label. e.g. jal x1, LOOP
            } else if (info.type == "I" && mnemonic != "lw" && ops.size() == 3) {
                // ADDI is rd, rs1, imm
            } else if (info.type == "R" && ops.size() == 3) {
                // ADD is rd, rs1, rs2
            } else if (mnemonic == "slli" && ops.size() == 3) {
                 // SLLI is rd, rs1, shamt
            } else {
                cerr << "ERROR at address 0x" << hex << inst.address << ": Wrong number of operands for " << mnemonic << ". Expected " << dec << expectedOps << ", got " << ops.size() << ". Line: " << inst.originalLine << endl;
                success = false;
            }
        }
        
        // 2. Register Range Check (simplified for all instructions that take register names)
        for (const string& op : ops) {
            if (op.length() > 1 && op[0] == 'x' && getRegisterNumber(op) == -1) {
                cerr << "ERROR at address 0x" << hex << inst.address << ": Invalid register number: " << op << ". Line: " << inst.originalLine << endl;
                success = false;
            }
        }

        // 3. Label Resolution Check for B and J types
        if (info.type == "B") {
            const string& label = ops[2]; // B-Type: rs1, rs2, label
            if (SYMBOL_TABLE.find(label) == SYMBOL_TABLE.end()) {
                cerr << "ERROR at address 0x" << hex << inst.address << ": Branch target label not found: " << label << ". Line: " << inst.originalLine << endl;
                success = false;
            }
        } else if (info.type == "J") {
            const string& label = ops[1]; // J-Type: rd, label
            if (SYMBOL_TABLE.find(label) == SYMBOL_TABLE.end()) {
                cerr << "ERROR at address 0x" << hex << inst.address << ": Jump target label not found: " << label << ". Line: " << inst.originalLine << endl;
                success = false;
            }
        }
    }
    return success;

}
