#include "LispNode.h"

#include "operators.h"
#include "extra.h"

LispNode::LispNode(LispType type): type{type}, head{nullptr} {
}

LispNode::~LispNode() {
	if(type == LispType::AtomPure || type == LispType::AtomBoolean || type == LispType::AtomString) {
		free(string);
	}

	if(type == LispType::List) {
		head = nullptr;
	}
}

bool LispNode::operator==(const LispNode &other) const {
	switch(type) {
		case AtomPure:
		case AtomBoolean:
		case AtomString:
			return (strcmp(string, other.string) == 0);
		case AtomCharacter:
		case AtomNumericIntegral:
			return (number_i == other.number_i);
		case AtomNumericReal:
			return (number_r == other.number_r);
		case List:
			return (this == &other);
		default:
			return false;
	}
}

bool LispNode::is_atom() const {
	return (type != LispType::List);
}

bool LispNode::is_list() const {
	return (type == LispType::List);
}

bool LispNode::is_pure() const {
	return (!is_boolean() && !is_numeric() && !is_string() && !is_character());
}

bool LispNode::is_boolean() const {
	return (type == LispType::AtomBoolean);
}

bool LispNode::is_string() const {
	return (type == LispType::AtomString);
}

bool LispNode::is_character() const {
	return (type == LispType::AtomCharacter);
}

bool LispNode::is_numeric() const {
	return (type == LispType::AtomNumericIntegral || type == LispType::AtomNumericReal);
}

bool LispNode::is_numeric_integral() const {
	return (type == LispType::AtomNumericIntegral);
}

bool LispNode::is_numeric_real() const {
	return (type == LispType::AtomNumericReal);
}

void LispNode::op_arithmetic(int operation, LispNodeRC &first, LispNodeRC &second) {
	is_numeric_integral() ? op_arithmetic_integer(operation, first, second) : op_arithmetic_real(operation, first, second);
}

bool LispNode::op_comparison(int operation, LispNodeRC &other) {
	return (is_numeric_integral() ? op_comparison_integer(operation, other) : op_comparison_real(operation, other));
}

void LispNode::op_arithmetic_integer(int operation, LispNodeRC &first, LispNodeRC &second) {
	switch(operation) {
		case OP_PLUS:
			number_i = (first->number_i + second->number_i);
			break;
		case OP_MINUS:
			number_i = (first->number_i - second->number_i);
			break;
		case OP_TIMES:
			number_i = (first->number_i * second->number_i);
			break;
		case OP_DIVIDE:
			number_i = (first->number_i / second->number_i);
			break;
	}
}

void LispNode::op_arithmetic_real(int operation, LispNodeRC &first, LispNodeRC &second) {
	switch(operation) {
		case OP_PLUS:
			number_r = (first->number_r + second->number_r);
			break;
		case OP_MINUS:
			number_r = (first->number_r - second->number_r);
			break;
		case OP_TIMES:
			number_r = (first->number_r * second->number_r);
			break;
		case OP_DIVIDE:
			number_r = (first->number_r / second->number_r);
			break;
	}
}

bool LispNode::op_comparison_integer(int operation, LispNodeRC &other) {
	bool comparison_result = false;

	switch(operation) {
		case OP_LESS:
			comparison_result = (number_i < other->number_i);
			break;
		case OP_BIGGER:
			comparison_result = (number_i > other->number_i);
			break;
		case OP_EQUAL:
			comparison_result = (number_i == other->number_i);
			break;
		case OP_LESS_EQUAL:
			comparison_result = (number_i <= other->number_i);
			break;
		case OP_BIGGER_EQUAL:
			comparison_result = (number_i >= other->number_i);
			break;
	}

	return comparison_result;
}

bool LispNode::op_comparison_real(int operation, LispNodeRC &other) {
	bool comparison_result = false;

	switch(operation) {
		case OP_LESS:
			comparison_result = (number_r < other->number_r);
			break;
		case OP_BIGGER:
			comparison_result = (number_r > other->number_r);
			break;
		case OP_EQUAL:
			comparison_result = (number_r == other->number_r);
			break;
		case OP_LESS_EQUAL:
			comparison_result = (number_r <= other->number_r);
			break;
		case OP_BIGGER_EQUAL:
			comparison_result = (number_r >= other->number_r);
			break;
	}

	return comparison_result;
}

void LispNode::promoteReal() {
	Integral integral = number_i;

	type = LispType::AtomNumericReal;
	number_r = integral;
}

void LispNode::demoteReal() {
	Real real = number_r;

	type = LispType::AtomNumericIntegral;
#ifdef TARGET_6502
	number_i = real.as_i();
#else
	number_i = real;
#endif /* TARGET_6502 */
}

void LispNode::print() const {
	switch(type) {
		case AtomPure:
		case AtomBoolean:
			fputs(string, stdout);
			break;
		case AtomString:
			fputs("\"", stdout);
			fputs(string, stdout);
			fputs("\"", stdout);
			break;
		case AtomCharacter:
			fputs("#\\", stdout);
			fputc(static_cast<int>(number_i), stdout);
			break;
		case AtomNumericIntegral:
			print_integral(number_i);
			break;
		case AtomNumericReal:
			print_real(number_r);
			break;
		case List:
			fputs("(", stdout);
			
			for(Box *current = get_head_pointer(); current != nullptr; current = current->get_next_pointer()) {
				current->item->print();

				if(current->next != nullptr) {
					fputs(" ", stdout);
				}
			}

			fputs(")", stdout);
	}
}

Box::Box(const LispNodeRC &item): item{item} {}