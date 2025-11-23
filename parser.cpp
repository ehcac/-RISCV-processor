#include "parser.hpp"

// Definition of the global data map (declared extern in assembler.hpp)
map<unsigned int, int32_t> DATA_SEGMENT;

/**
 * Reads the file, ignores comments, and collects non-empty lines.
 * UPDATED: Now keeps lines starting with '.' (directives like .data, .text)
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
            // UPDATED: We NO LONGER ignore .word and .text lines here.
            // We need them for section tracking.
            lines.push_back(line);
        }
    }
    
    return lines;
}

/**
 * Pass 1: build the Symbol Table.
 * UPDATED: Handles separate counters for .text (0x80) and .data (0x00)
 */
map<string, unsigned int> buildSymbolTable(const vector<string>& lines) {
    map<string, unsigned int> symbolTable;
    unsigned int textAddress = INSTRUCTION_MEMORY_START; // 0x80
    unsigned int dataAddress = DATA_MEMORY_START;        // 0x00
    bool inDataSegment = false; // Default to text

    for (const string& line : lines) {
        string tempLine = line;

        // Handle Section Directives
        if (tempLine == ".data") { inDataSegment = true; continue; }
        if (tempLine == ".text") { inDataSegment = false; continue; }
        if (tempLine.find(".global") != string::npos) continue; 

        // Check for a label (ends with ':')
        size_t labelPos = tempLine.find(':');
        if (labelPos != string::npos) {
            // Extract label name (before the ':')
            string label = tempLine.substr(0, labelPos);
            label.erase(remove_if(label.begin(), label.end(), ::isspace), label.end());
            
            if (symbolTable.count(label)) {
                cerr << "ERROR: Duplicate label definition: " << label << endl;
                exit(1);
            }
            
            // Assign address based on current section
            if (inDataSegment) {
                symbolTable[label] = dataAddress;
            } else {
                symbolTable[label] = textAddress;
            }
            
            // Process the rest of the line (e.g., "label: .word 5" or "label: add...")
            tempLine = tempLine.substr(labelPos + 1);
            tempLine.erase(0, tempLine.find_first_not_of(" \t\r\n"));
        }

        if (tempLine.empty()) continue;

        // Increment Address Counter
        if (inDataSegment) {
            // Check for .word directive (allocates 4 bytes)
            stringstream ss(tempLine);
            string firstWord;
            ss >> firstWord;
            if (firstWord == ".word") {
                dataAddress += 4;
            }
        } else {
            // Instructions in .text segment take 4 bytes
            textAddress += 4;
        }
    }
    return symbolTable;
}

/**
 * NEW FUNCTION: Extracts values from .word directives in the .data section
 */
void parseDataSection(const vector<string>& lines) {
    unsigned int currentAddress = DATA_MEMORY_START; // 0x00
    bool inDataSegment = false;

    for (const string& line : lines) {
        string tempLine = line;

        // Handle Section Directives
        if (tempLine == ".data") { inDataSegment = true; continue; }
        if (tempLine == ".text") { inDataSegment = false; continue; }

        // Remove label if present
        size_t labelPos = tempLine.find(':');
        if (labelPos != string::npos) {
            tempLine = tempLine.substr(labelPos + 1);
            tempLine.erase(0, tempLine.find_first_not_of(" \t\r\n"));
        }

        if (tempLine.empty()) continue;

        if (inDataSegment) {
            stringstream ss(tempLine);
            string directive;
            ss >> directive;
            
            if (directive == ".word") {
                string valueStr;
                ss >> valueStr; // Read the value after .word
                int value = getImmediateValue(valueStr); // Use util function to parse hex/dec
                
                // Store in global data map
                DATA_SEGMENT[currentAddress] = value;
                currentAddress += 4;
            }
        }
    }
}

// Assembler Phase 2: Parsing, Validation & Encoding Setup

/**
 * Pass 2: Parses instructions and prepares them for encoding.
 * UPDATED: Now skips over .data sections and directives.
 */
vector<ParsedInstruction> parseInstructions(const vector<string>& lines) {
    vector<ParsedInstruction> instructions;
    unsigned int currentAddress = INSTRUCTION_MEMORY_START;
    bool inTextSegment = true; // Assume start in text unless .data seen first

    // Initial scan to set correct start state if file starts with .data
    bool hasDirectives = false;
    for(const auto& l : lines) if(l == ".text" || l == ".data") hasDirectives = true;
    if(!hasDirectives) inTextSegment = true;

    for (const string& line : lines) {
        string currentLine = line;
        
        // Handle Section switching
        if (currentLine == ".data") { inTextSegment = false; continue; }
        if (currentLine == ".text") { inTextSegment = true; continue; }
        if (currentLine.find(".global") != string::npos) continue;

        // If we are in the data segment, DO NOT parse as instructions
        if (!inTextSegment) continue;

        size_t labelPos = currentLine.find(':');
        if (labelPos != string::npos) {
            currentLine = currentLine.substr(labelPos + 1);
            currentLine.erase(0, currentLine.find_first_not_of(" \t\r\n"));
            if (currentLine.empty()) {
                continue; 
            }
        }

        // Check for directives inside .text (like .word shouldn't be here usually, but safety check)
        if (currentLine[0] == '.') continue;

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