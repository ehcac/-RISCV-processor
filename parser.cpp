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
