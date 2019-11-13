
#include "Decompile.h"
#include "Regex.h"

namespace {

constexpr int16_t make_int16(uint8_t hi, uint16_t lo) {
	return (static_cast<int16_t>(hi << 8)) | lo;
}

constexpr uint16_t make_uint16(uint8_t hi, uint16_t lo) {
	return (static_cast<uint16_t>(hi << 8)) | lo;
}

}

std::vector<Instruction> decompileRegex(const Regex &re) {
	std::vector<Instruction> results;

	if (!re.isValid()) {
		return results;
	}

	if (re.program.size() < 3) {
		return results;
	}

	auto it = re.program.begin();

	++it;                   // skip magic byte
	uint8_t parens = *it++; // read paren count
	uint8_t braces = *it++; // read the braces

	(void)parens;
	(void)braces;

	while (it != re.program.end()) {
		uint8_t opcode = *it++;
		switch (opcode) {
		case END: {
			/* 01 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case BOL: {
			/* 02 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case EOL: {
			/* 03 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case BOWORD: {
			/* 04 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case EOWORD: {
			/* 05 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case NOT_BOUNDARY: {
			/* 06 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case EXACTLY: {
			/* 07 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;
			std::string set;
			while (*it != '\0') {
				set.push_back(static_cast<char>(*it));
				++it;
			}
			++it;

			results.emplace_back(Instruction2{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo), set});
			break;
		}
		case SIMILAR: {
			/* 08 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;
			std::string set;
			while (*it != '\0') {
				set.push_back(static_cast<char>(*it));
				++it;
			}
			++it;

			results.emplace_back(Instruction2{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo), set});
			break;
		}
		case ANY_OF: {
			/* 09 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;
			std::string set;
			while (*it != '\0') {
				set.push_back(static_cast<char>(*it));
				++it;
			}
			++it;

			results.emplace_back(Instruction2{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo), set});
			break;
		}
		case ANY_BUT: {
			/* 10 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;
			std::string set;
			while (*it != '\0') {
				set.push_back(static_cast<char>(*it));
				++it;
			}
			++it;

			results.emplace_back(Instruction2{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo), set});
			break;
		}
		case ANY: {
			/* 11 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case EVERY: {
			/* 12 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case DIGIT: {
			/* 13 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case NOT_DIGIT: {
			/* 14 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case LETTER: {
			/* 15 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case NOT_LETTER: {
			/* 16 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case SPACE: {
			/* 17 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case SPACE_NL: {
			/* 18 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case NOT_SPACE: {
			/* 19 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case NOT_SPACE_NL: {
			/* 20 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case WORD_CHAR: {
			/* 21 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case NOT_WORD_CHAR: {
			/* 22 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case IS_DELIM: {
			/* 23 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case NOT_DELIM: {
			/* 24 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case STAR: {
			/* 25 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case LAZY_STAR: {
			/* 26 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case QUESTION: {
			/* 27 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case LAZY_QUESTION: {
			/* 28 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case PLUS: {
			/* 29 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case LAZY_PLUS: {
			/* 30 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case BRACE: {
			/* 31 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			int16_t min_hi = *it++;
			int16_t min_lo = *it++;

			int16_t max_hi = *it++;
			int16_t max_lo = *it++;

			results.emplace_back(Instruction3{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo), make_uint16(min_hi, min_lo), make_uint16(max_hi, max_lo)});
			break;
		}
		case LAZY_BRACE: {
			/* 32 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			int16_t min_hi = *it++;
			int16_t min_lo = *it++;

			int16_t max_hi = *it++;
			int16_t max_lo = *it++;

			results.emplace_back(Instruction3{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo), make_uint16(min_hi, min_lo), make_uint16(max_hi, max_lo)});
			break;
		}
		case NOTHING: {
			/* 33 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case BRANCH: {
			/* 34 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case BACK: {
			/* 35 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case INIT_COUNT: {
			/* 36 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;
			uint8_t operand   = *it++;

			results.emplace_back(Instruction4{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo), operand});
			break;
		}
		case INC_COUNT: {
			/* 37 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;
			uint8_t operand   = *it++;

			results.emplace_back(Instruction4{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo), operand});
			break;
		}
		case TEST_COUNT: {
			/* 38 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;
			uint8_t index     = *it++;
			uint8_t test_hi   = *it++;
			uint8_t test_lo   = *it++;

			results.emplace_back(Instruction5{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo), index, make_uint16(test_hi, test_lo)});
			break;
		}
		case BACK_REF: {
			/* 39 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;
			uint8_t index     = *it++;

			results.emplace_back(Instruction4{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo), index});
			break;
		}
		case BACK_REF_CI: {
			/* 40 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;
			uint8_t index     = *it++;

			results.emplace_back(Instruction4{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo), index});
			break;
		}
		case X_REGEX_BR:
			/* 41 */
			abort();
		case X_REGEX_BR_CI:
			/* 42 */
			abort();
		case POS_AHEAD_OPEN: {
			/* 43 */
			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case NEG_AHEAD_OPEN: {
			/* 44 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case LOOK_AHEAD_CLOSE: {
			/* 45 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case POS_BEHIND_OPEN: {
			/* 46 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;
			uint8_t op1_hi    = *it++;
			uint8_t op1_lo    = *it++;
			uint8_t op2_hi    = *it++;
			uint8_t op2_lo    = *it++;

			results.emplace_back(Instruction6{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo), make_uint16(op1_hi, op1_lo), make_uint16(op2_hi, op2_lo)});
			break;
		}
		case NEG_BEHIND_OPEN: {
			/* 47 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;
			uint8_t op1_hi    = *it++;
			uint8_t op1_lo    = *it++;
			uint8_t op2_hi    = *it++;
			uint8_t op2_lo    = *it++;

			results.emplace_back(Instruction6{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo), make_uint16(op1_hi, op1_lo), make_uint16(op2_hi, op2_lo)});
			break;
		}
		case LOOK_BEHIND_CLOSE: {
			/* 48 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case OPEN:
		case OPEN + 1:
		case OPEN + 2:
		case OPEN + 3:
		case OPEN + 4:
		case OPEN + 5:
		case OPEN + 6:
		case OPEN + 7:
		case OPEN + 8:
		case OPEN + 9:
		case OPEN + 10:
		case OPEN + 11:
		case OPEN + 12:
		case OPEN + 13:
		case OPEN + 14:
		case OPEN + 15:
		case OPEN + 16:
		case OPEN + 17:
		case OPEN + 18:
		case OPEN + 19:
		case OPEN + 20:
		case OPEN + 21:
		case OPEN + 22:
		case OPEN + 23:
		case OPEN + 24:
		case OPEN + 25:
		case OPEN + 26:
		case OPEN + 27:
		case OPEN + 28:
		case OPEN + 29:
		case OPEN + 30:
		case OPEN + 31:
		case OPEN + 32:
		case OPEN + 33:
		case OPEN + 34:
		case OPEN + 35:
		case OPEN + 36:
		case OPEN + 37:
		case OPEN + 38:
		case OPEN + 39:
		case OPEN + 40:
		case OPEN + 41:
		case OPEN + 42:
		case OPEN + 43:
		case OPEN + 44:
		case OPEN + 45:
		case OPEN + 46:
		case OPEN + 47:
		case OPEN + 48:
		case OPEN + 49: {
			/* 49-98 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		case CLOSE:
		case CLOSE + 1:
		case CLOSE + 2:
		case CLOSE + 3:
		case CLOSE + 4:
		case CLOSE + 5:
		case CLOSE + 6:
		case CLOSE + 7:
		case CLOSE + 8:
		case CLOSE + 9:
		case CLOSE + 10:
		case CLOSE + 11:
		case CLOSE + 12:
		case CLOSE + 13:
		case CLOSE + 14:
		case CLOSE + 15:
		case CLOSE + 16:
		case CLOSE + 17:
		case CLOSE + 18:
		case CLOSE + 19:
		case CLOSE + 20:
		case CLOSE + 21:
		case CLOSE + 22:
		case CLOSE + 23:
		case CLOSE + 24:
		case CLOSE + 25:
		case CLOSE + 26:
		case CLOSE + 27:
		case CLOSE + 28:
		case CLOSE + 29:
		case CLOSE + 30:
		case CLOSE + 31:
		case CLOSE + 32:
		case CLOSE + 33:
		case CLOSE + 34:
		case CLOSE + 35:
		case CLOSE + 36:
		case CLOSE + 37:
		case CLOSE + 38:
		case CLOSE + 39:
		case CLOSE + 40:
		case CLOSE + 41:
		case CLOSE + 42:
		case CLOSE + 43:
		case CLOSE + 44:
		case CLOSE + 45:
		case CLOSE + 46:
		case CLOSE + 47:
		case CLOSE + 48:
		case CLOSE + 49: {
			/* 99-148 */

			uint8_t offset_hi = *it++;
			uint8_t offset_lo = *it++;

			results.emplace_back(Instruction1{static_cast<Opcode>(opcode), make_int16(offset_hi, offset_lo)});
			break;
		}
		default:
			assert(0);
		}
	}

	return results;
}
