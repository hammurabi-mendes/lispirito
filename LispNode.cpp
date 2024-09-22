#include "LispNode.h"

#include "extra.h"

LispNode::LispNode(LispType type): type{type} {
	if(type == LispType::List) {
		head = new BoxRC(nullptr);
	}
}

LispNode::~LispNode() {
	if(type == LispType::AtomPure || type == LispType::AtomBoolean || type == LispType::AtomString) {
		free(string);
	}

	if(type == LispType::List) {
		delete head;
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

bool LispNode::is_atom() {
	return (type != LispType::List);
}

bool LispNode::is_list() {
	return (type == LispType::List);
}

bool LispNode::is_pure() {
	return (!is_boolean() && !is_numeric() && !is_string() && !is_character());
}

bool LispNode::is_boolean() {
	return (type == LispType::AtomBoolean);
}

bool LispNode::is_string() {
	return (type == LispType::AtomString);
}

bool LispNode::is_character() {
	return (type == LispType::AtomCharacter);
}

bool LispNode::is_numeric() {
	return (type == LispType::AtomNumericIntegral || type == LispType::AtomNumericReal);
}

bool LispNode::is_numeric_integral() {
	return (type == LispType::AtomNumericIntegral);
}

bool LispNode::is_numeric_real() {
	return (type == LispType::AtomNumericReal);
}

void LispNode::print() {
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