#ifndef ASSEMBLER_HPP
#define ASSEMBLER_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <iomanip>
#include <algorithm>
#include <cmath>

using namespace std;

const unsigned int INSTRUCTION_MEMORY_START = 0x80;
const unsigned int DATA_MEMORY_START = 0x00; // Data starts at 0

struct InstructionInfo {
    string type, op, f3, f7;
    InstructionInfo(string t, string o, string f) : type(t), op(o), f3(f), f7("") {}
    InstructionInfo(string t, string o, string f3_val, string f7_val)
        : type(t), op(o), f3(f3_val), f7(f7_val) {}
};

struct ParsedInstruction {
    string mnemonic;
    vector<string> operands;
    unsigned int address;
    string originalLine;
};

extern map<string, InstructionInfo> INSTRUCTION_SET;
extern map<unsigned int, unsigned int> INSTRUCTION_MEMORY;
extern map<string, unsigned int> SYMBOL_TABLE;
extern map<unsigned int, int32_t> DATA_SEGMENT; 

#endif