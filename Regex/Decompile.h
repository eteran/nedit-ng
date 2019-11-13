
#ifndef DECOMPILE_H_
#define DECOMPILE_H_

#include "Opcodes.h"
#include <boost/variant.hpp>
#include <string>
#include <vector>
class Regex;

struct Instruction1 {
	Opcode opcode;
	int16_t offset;
};

struct Instruction2 {
	Opcode opcode;
	int16_t offset;
	std::string set;
};

struct Instruction3 {
	Opcode opcode;
	int16_t offset;
	uint16_t min;
	uint16_t max;
};

struct Instruction4 {
	Opcode opcode;
	int16_t offset;
	uint16_t index;
};

struct Instruction5 {
	Opcode opcode;
	int16_t offset;
	uint16_t index;
	uint16_t test;
};

struct Instruction6 {
	Opcode opcode;
	int16_t offset;
	uint16_t op1;
	uint16_t op2;
};

using Instruction = boost::variant<Instruction1, Instruction2, Instruction3, Instruction4, Instruction5, Instruction6>;

std::vector<Instruction> decompileRegex(const Regex &re);

#endif
