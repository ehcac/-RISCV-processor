#ifndef PARSER_HPP
#define PARSER_HPP

#include "assembler.hpp"
#include "utils.hpp"

vector<string> readAndPreprocess(const string& filename);
map<string, unsigned int> buildSymbolTable(const vector<string>& lines);
vector<ParsedInstruction> parseInstructions(const vector<string>& lines);
bool validateInstructions(const vector<ParsedInstruction>& instructions);
void parseDataSection(const vector<string>& lines);

#endif
