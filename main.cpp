#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "operators.h"
#include "lambdas.h"
#include "macros.h"
#include "extra.h"
#include "LispNode.h"

constexpr unsigned int MAX_EXPRESSION_SIZE = 256;
constexpr unsigned int MAX_TOKEN_SIZE = 256;

constexpr int PARSE_CHARACTER = 0x1;
constexpr int PARSE_QUOTED = 0x2;
constexpr int PARSE_ALPHA = 0x4;
constexpr int PARSE_DIGIT = 0x8;
constexpr int PARSE_DOT = 0x10;

// Global constants
LispNodeRC atom_true;
LispNodeRC atom_false;
LispNodeRC list_empty;

// Global environment
LispNodeRC global_environment;

// Environment to modify upon defines (only changed upon begin statements)
LispNodeRC *context_environment;

// Forward declarations
char *read_expression();
LispNodeRC parse_expression(char *buffer);
LispNodeRC eval_expression(LispNodeRC input, LispNodeRC environment);

#ifdef TARGET_6502
	#ifdef SIMPLE_ALLOCATOR
		#include "SimpleAllocator.h"

		SimpleAllocator allocator;

        #define Allocate allocator.malloc
        #define Deallocate allocator.free
	#else
		#define Allocate malloc
		#define Deallocate free
	#endif /* SIMPLE_ALLOCATOR */
#else
	#define Allocate malloc
	#define Deallocate free
#endif /* TARGET_6502 */

void *operator new(size_t size) {
	CounterType *pointer = (CounterType *) Allocate(size + sizeof(CounterType));

	*pointer = 0;

	return pointer + 1;
}

void operator delete(void *pointer) noexcept {
	Deallocate(((char *) pointer) - sizeof(CounterType));
}

// Utility functions

void print_error(const char *op, const char *message) {
	fputs(op, stdout);
	fputs(": ", stdout);
	fputs(message, stdout);
}

int get_operation_index(const char *string) {
	for(int i = 0; i < NUMBER_BASIC_OPERATORS; i++) {
		if(strcmp(string, operator_names[i]) == 0) {
			return i;
		}
	}

	return -1;
}

// Make operators

LispNodeRC make_operator(char *operator_name) {
	LispNodeRC result = new LispNode(LispType::AtomPure);
	result->string = strdup(operator_name);

	return result;
}

LispNodeRC make1(const LispNodeRC &first) {
	LispNodeRC result = new LispNode(LispType::List);

	result->head = new Box(first);

	return result;
}

LispNodeRC make2(const LispNodeRC &first, const LispNodeRC &second) {
	LispNodeRC result = new LispNode(LispType::List);

	result->head = new Box(first);
	result->head->next = new Box(second);

	return result;
}

LispNodeRC make3(const LispNodeRC &first, const LispNodeRC &second, const LispNodeRC &third) {
	LispNodeRC result = new LispNode(LispType::List);

	result->head = new Box(first);
	result->head->next = new Box(second);
	result->head->next->next = new Box(third);

	return result;
}

LispNodeRC make_cons(const LispNodeRC &first, const LispNodeRC &second) {
	LispNodeRC result = new LispNode(LispType::List);

	result->head = new Box(first);

	if(second->is_list()) {
		result->head->next = second->head;
	}
	else {
		result->head->next = new Box(second);
	}

	return result;
}

LispNodeRC make_car(const LispNodeRC &list) {
	return list->head->item;
}

LispNodeRC make_cdr(const LispNodeRC &list) {
	const BoxRC &second_element_box = list->head->next;

	if(second_element_box == nullptr) {
		return list_empty;
	}

	LispNodeRC result = new LispNode(LispType::List);
	result->head = second_element_box;

	return result;
}

LispNodeRC make_query_optional_replace(const LispNodeRC &term, const LispNodeRC &list, const LispNodeRC &replacement = nullptr) {
	for(Box *current_definition_box = list->get_head_pointer(); current_definition_box != nullptr; current_definition_box = current_definition_box->get_next_pointer()) {
		const LispNodeRC &current_pair = current_definition_box->item;

		const LispNodeRC &key = current_pair->head->item;
		const LispNodeRC &value = current_pair->head->next->item;

		if(*term == *key) {
			if(replacement.get_pointer() != nullptr) {
				LispNodeRC &value_noconst = current_pair->head->next->item;
				value_noconst = replacement;
			}

			return value;
		}
	}

	return nullptr;
}

LispNodeRC make_substitution(const LispNodeRC &old_symbol, const LispNodeRC &new_symbol, const LispNodeRC &expression) {
	if(expression->is_atom()) {
		return (expression == old_symbol) ? new_symbol : expression;
	}

	LispNodeRC output = new LispNode(LispType::List);

	Box *last_box = nullptr;

	for(Box *current_expression_box = expression->get_head_pointer(); current_expression_box != nullptr; current_expression_box = current_expression_box->get_next_pointer()) {
		LispNodeRC substituted = make_substitution(old_symbol, new_symbol, current_expression_box->item);

		Box *substituted_box = new Box(substituted); 

		if(last_box == nullptr) {
			output->head = substituted_box;
		}
		else {
			last_box->next = substituted_box;
		}

		last_box = substituted_box;
	}

	// The last output is the result of the expression
	return output;
}

char *read_expression() {
	static char read_buffer[MAX_EXPRESSION_SIZE];

	int nread = 0;
	int total_open = 0;
	int total_close = 0;

	while(true) {
		char *last_line;

		if((last_line = fgets(read_buffer + nread, MAX_EXPRESSION_SIZE - nread, stdin)) == nullptr) {
			break;
		}

		for(char *current = last_line; *current != '\0'; current++) {
			if(*current == '(') {
				total_open++;
			}
			if(*current == ')') {
				total_close++;
			}

			*current = tolower(*current);

			nread++;
		}

		if(total_open <= total_close) {
			break;
		}
	}

	if(nread == 0) {
		return nullptr;
	}

	return read_buffer;
}

char *get_next_token(char *buffer, size_t buffer_length, size_t &position) {
	static char result[MAX_EXPRESSION_SIZE];

	// If buffer is exhausted, return null
	if(position >= buffer_length) {
		return nullptr;
	}

	char current = buffer[position];

	// Skip starting blank characters
	while(current != '\0' && isspace(current)) {
		position++;
		current = buffer[position];
	}

	// If nothing is remaining, return null
	if(current == '\0') {
		return nullptr;
	}

	// If next token is a parenthesis or quote, return it
	if(current == '(' || current == ')' || current == '\'') {
		result[0] = current;
		result[1] = '\0';

		position++;

		return result;
	}

	// Else if next token is something else, return it
	int result_position = 0;

	do {
		// Collect the non-space, non-parenthesis, non-quote character
		result[result_position] = current;
		result_position++;

		// Go to next character
		position++;
		current = buffer[position];
	} while(current != '\0' && current != '(' && current != ')' && current != '\'' && !isspace(current));

	result[result_position] = '\0';

	return result;
}

int examine_string(char *token) {
	int output = 0;

	int token_length = strlen(token);

	if(token_length == 0) {
		return output;
	}

	if(token_length >= 2 && token[0] == '#' && token[1] == '\\') {
		output |= PARSE_CHARACTER;

		return output;
	}

	if(token[0] == '\"' && token[token_length - 1] == '\"') {
		output |= PARSE_QUOTED;

		return output;
	}

	if(token[0] == '\'' && token[token_length - 1] == '\'') {
		output |= PARSE_QUOTED;

		return output;
	}

	for(auto i = 0; i < token_length; i++) {
		char current = token[i];

		if(isalpha(current)) {
			output |= PARSE_ALPHA;
		}

		if(isdigit(current)) {
			output |= PARSE_DIGIT;
		}

		if(current == '.') {
			output |= PARSE_DOT;
		}
	}

	return output;
}

LispNodeRC parse_atom(char *token) {
	int output = examine_string(token);

	if(output & PARSE_CHARACTER) {
		LispNodeRC result = new LispNode(LispType::AtomCharacter);

		result->number_i = token[2];

		return result;
	}

	if(output & PARSE_QUOTED) {
		LispNodeRC result = new LispNode(LispType::AtomString);

		// Remove quotes
		token[strlen(token) - 1] = '\0';
		token++;

		result->string = strdup(token);

		return result;
	}

	if(!(output & PARSE_ALPHA) && (output & PARSE_DIGIT) && (output & PARSE_DOT)) {
		LispNodeRC result = new LispNode(LispType::AtomNumericReal);

		result->number_r = atof(token);

		return result;
	}

	if(!(output & PARSE_ALPHA) && (output & PARSE_DIGIT) && !(output & PARSE_DOT)) {
		LispNodeRC result = new LispNode(LispType::AtomNumericIntegral);

		result->number_i = atol(token);

		return result;
	}

	// Pure atoms

	if(strcmp(token, "#t") == 0) {
		return atom_true;	
	}

	if(strcmp(token, "#f") == 0) {
		return atom_false;	
	}

	LispNodeRC result = new LispNode(LispType::AtomPure);

	result->string = strdup(token);

	return result;
}

LispNodeRC parse_expression(char *buffer, size_t buffer_length, size_t &position, bool &error) {
	char *token = get_next_token(buffer, buffer_length, position);

	if(token == nullptr) {
		error = true;

		return nullptr;
	}

	if(strcmp(token, "'") == 0) {
		LispNodeRC result = new LispNode(LispType::List);

		LispNodeRC quote = make_operator("quote");
		LispNodeRC quoted = parse_expression(buffer, buffer_length, position, error);

		if(error) {
			return nullptr;
		}

		return make2(quote, quoted);
	}
	
	if(strcmp(token, "(") == 0) {
		LispNodeRC result = new LispNode(LispType::List);
		result->head = nullptr;

		Box *last_box = nullptr;
		LispNodeRC member;

		while((member = parse_expression(buffer, buffer_length, position, error)) != nullptr) {
			Box *member_box = new Box(member);

			if(last_box == nullptr) {
				result->head = member_box;
			}
			else {
				last_box->next = member_box;
			}

			last_box = member_box;
		}

		if(error) {
			return nullptr;
		}

		if(result->head == nullptr) {
			return list_empty;
		}

		return result;
	}

	if(strcmp(token, ")") == 0) {
		return nullptr;
	}

	return parse_atom(token);
}

LispNodeRC parse_expression(char *buffer) {
	// Initial token position
	size_t position = 0;

	// Initial error flag (can change if no token is found)
	bool error = false;

	LispNodeRC result = parse_expression(buffer, strlen(buffer), position, error);

	if(error == true) {
		return nullptr;
	}

	return result;
}

// Helper functions

size_t count_members(const LispNodeRC &list) {
	size_t count = 0;

	for(Box *current_box = list->get_head_pointer(); current_box != nullptr; current_box = current_box->get_next_pointer()) {
		count++;
	}

	return count;
}

// Eval functions

LispNodeRC eval_quote(LispNodeRC input, LispNodeRC environment) {
	if(count_members(input) != 2) {
		print_error("quote", "missing arguments\n");

		return nullptr;
	}

	LispNodeRC argument = input->head->next->item;

	return argument;
}

LispNodeRC eval_cond(LispNodeRC input, LispNodeRC environment) {
	if(count_members(input) != 2) {
		print_error("cond", "missing arguments\n");

		return nullptr;
	}

	const LispNodeRC &argument1 = input->head->next->item;

	for(Box *current_box = argument1->get_head_pointer(); current_box != nullptr; current_box = current_box->get_next_pointer()) {
		if(!current_box->item->is_list()) {
			print_error("cond", "argument type error\n");

			return nullptr;
		}

		if(count_members(current_box->item) != 2) {
			print_error("cond", "argument type error\n");

			return nullptr;
		}

		const LispNodeRC &condition = current_box->item->head->item;
		const LispNodeRC &resolution = current_box->item->head->next->item;

		LispNodeRC result = eval_expression(condition, environment);

		if(result == nullptr) {
			return nullptr;
		}

		if(result == atom_true) {
			return eval_expression(resolution, environment);	
		}
	}

	print_error("cond", "no valid clause found\n");
	return nullptr;
}

bool check_arithmethic_and_promote(char *operator_name, const LispNodeRC &first, const LispNodeRC &second) {
	if(!first->is_numeric() || !second->is_numeric()) {
		print_error(operator_name, "argument type error\n");

		return false;
	}

	if(first->is_numeric_real() && !second->is_numeric_real()) {
		Integral integral = second->number_i;

		second->type = LispType::AtomNumericReal;
		second->number_r = integral;
	}

	if(!first->is_numeric_real() && second->is_numeric_real()) {
		Integral integral = first->number_i;

		first->type = LispType::AtomNumericReal;
		first->number_r = integral;
	}

	return true;
}

LispNodeRC eval_gen0(LispNodeRC input, LispNodeRC environment) {
	const LispNodeRC &operator_name = input->head->item;

	int operation_index = get_operation_index(operator_name->string);

	switch(operation_index) {
		case OP_READ: {
			char *input_string = read_expression();

			if(input_string == nullptr) {
				break;
			}

			return parse_expression(input_string);
		}
    	case OP_NEWLINE:
			fputs("\n", stdout);

			return list_empty;
		case OP_CURRENT_ENVIRONMENT:
			return environment;
		default:
			fputs("PANIC: invalid type operator requested\n", stdout);
			exit(EXIT_FAILURE);
	}

	// Unreachable
	return nullptr;
}

LispNodeRC eval_gen1(LispNodeRC input, LispNodeRC environment) {
	const LispNodeRC &operator_name = input->head->item;

	if(count_members(input) != 2) {
		print_error(operator_name->string, "missing arguments\n");

		return nullptr;
	}

	const LispNodeRC &argument1 = input->head->next->item;
	LispNodeRC output1 = eval_expression(argument1, environment);

	if(output1 == nullptr) {
		return nullptr;
	}

	int operation_index = get_operation_index(operator_name->string);
	LispNodeRC result = nullptr;

	if(operation_index == OP_CAR || operation_index == OP_CDR) {
		if(!output1->is_list() || output1->head == nullptr) {
			print_error("car/cdr", "argument type error\n");

			return nullptr;
		}
	}

	switch(operation_index) {
		case OP_CAR:
			return make_car(output1);
		case OP_CDR:
			return make_cdr(output1);
		case OP_ATOM_Q:
			return output1->is_atom() ? atom_true : atom_false;
		case OP_PAIR_Q:
			return (output1->is_atom() || output1 == list_empty) ? atom_false : atom_true;
		case OP_CHAR_Q:
			return output1->is_character() ? atom_true : atom_false;
		case OP_BOOLEAN_Q:
			return output1->is_boolean() ? atom_true : atom_false;
		case OP_STRING_Q:
			return output1->is_string() ? atom_true : atom_false;
		case OP_NUMBER_Q:
			return output1->is_numeric() ? atom_true : atom_false;
		case OP_INTEGER_Q:
			return output1->is_numeric_integral() ? atom_true : atom_false;
		case OP_REAL_Q:
			return output1->is_numeric_real() ? atom_true : atom_false;
		case OP_INTEGER_REAL:
			if(!output1->is_numeric_integral()) {
				print_error("integer->real", "argument type error\n");

				return nullptr;
			}

			output1->type = LispType::AtomNumericReal;
			output1->number_r = output1->number_i;

			return output1;
		case OP_REAL_INTEGER:
			if(!output1->is_numeric_real()) {
				print_error("real->integer", "argument type error\n");

				return nullptr;
			}

			output1->type = LispType::AtomNumericIntegral;
#ifdef TARGET_6502
			output1->number_i = output1->number_r.as_f();
#else
			output1->number_i = output1->number_r;
#endif /* TARGET_6502 */

			return output1;
		case OP_NOT:
			if(!output1->is_boolean()) {
				print_error("not", "argument type error\n");

				return nullptr;
			}

			return (output1 == atom_true ? atom_false : atom_true);
		case OP_INTEGER_CHAR:
			if(!output1->is_numeric_integral()) {
				print_error("integer->char", "argument type error\n");

				return nullptr;
			}

			output1->type = AtomCharacter;

			return output1;
    	case OP_CHAR_INTEGER:
			if(!output1->is_character()) {
				print_error("char->integer", "argument type error\n");

				return nullptr;
			}

			output1->type = AtomNumericIntegral;

			return output1;
    	case OP_NUMBER_STRING:
			if(!output1->is_numeric()) {
				print_error("number->string", "argument type error\n");

				return nullptr;
			}

			char string_buffer[MAX_NUMERIC_STRING_LENGTH];

			if(output1->is_numeric_integral()) {
				get_integral_string(output1->number_i, string_buffer);
			}
			else if(output1->is_numeric_real()) {
				get_real_string(output1->number_r, string_buffer);
			}

			result = new LispNode(LispType::AtomString);

			result->string = strdup(string_buffer);

			break;
    	case OP_STRING_NUMBER:
			if(!output1->is_string()) {
				print_error("string->number", "argument type error\n");

				return nullptr;
			}

			result = parse_atom(output1->string);

			if(result == nullptr || !result->is_numeric()) {
				print_error("string->number", "argument type error\n");

				return nullptr;
			}

			break;
    	case OP_STRING_LENGTH:
			if(!output1->is_string()) {
				print_error("string-length", "argument type error\n");

				return nullptr;
			}

			result = new LispNode(LispType::AtomNumericIntegral);

			result->number_i = strlen(output1->string);

			break;
    	case OP_DISPLAY:
    	case OP_WRITE:
			output1->print();

			return list_empty;
		default:
			fputs("PANIC: invalid type operator requested\n", stdout);
			exit(EXIT_FAILURE);
	}

	return result;
}

LispNodeRC eval_gen2(LispNodeRC input, LispNodeRC environment) {
	const LispNodeRC &operator_name = input->head->item;

	if(count_members(input) != 3) {
		print_error(operator_name->string, "missing arguments\n");

		return nullptr;
	}

	const LispNodeRC &argument1 = input->head->next->item;
	const LispNodeRC &argument2 = input->head->next->next->item;

	LispNodeRC output1 = eval_expression(argument1, environment);
	LispNodeRC output2 = eval_expression(argument2, environment);

	if(output1 == nullptr || output2 == nullptr) {
		return nullptr;
	}

	int operation_index = get_operation_index(operator_name->string);
	LispNodeRC result = nullptr;

	if(operation_index >= OP_PLUS && operation_index <= OP_BIGGER_EQUAL) {
		if(!check_arithmethic_and_promote(operator_name->string, output1, output2)) {
			return nullptr;
		}

		result = new LispNode(output1->type);
	}

	switch(operation_index) {
		case OP_CONS:
			return make_cons(output1, output2);
		case OP_EQ_Q:
			return (*output1 == *output2) ?  atom_true : atom_false;
		case OP_PLUS:
			if(output1->type == LispType::AtomNumericIntegral) {
				result->number_i = (output1->number_i + output2->number_i);
			}
			else {
				result->number_r = (output1->number_r + output2->number_r);
			}

			break;
		case OP_MINUS:
			if(output1->type == LispType::AtomNumericIntegral) {
				result->number_i = (output1->number_i - output2->number_i);
			}
			else {
				result->number_r = (output1->number_r - output2->number_r);
			}

			break;
		case OP_TIMES:
			if(output1->type == LispType::AtomNumericIntegral) {
				result->number_i = (output1->number_i * output2->number_i);
			}
			else {
				result->number_r = (output1->number_r * output2->number_r);
			}

			break;
		case OP_DIVIDE:
			if(output1->type == LispType::AtomNumericIntegral) {
				result->number_i = (output1->number_i / output2->number_i);
			}
			else {
				result->number_r = (output1->number_r / output2->number_r);
			}

			break;
		case OP_LESS:
			if(output1->type == LispType::AtomNumericIntegral) {
				return (output1->number_i < output2->number_i) ? atom_true : atom_false;
			}
			else {
				return (output1->number_r < output2->number_r) ? atom_true : atom_false;
			}
		case OP_BIGGER:
			if(output1->type == LispType::AtomNumericIntegral) {
				return (output1->number_i > output2->number_i) ? atom_true : atom_false;
			}
			else {
				return (output1->number_r > output2->number_r) ? atom_true : atom_false;
			}
		case OP_EQUAL:
			if(output1->type == LispType::AtomNumericIntegral) {
				return (output1->number_i == output2->number_i) ? atom_true : atom_false;
			}
			else {
				return (output1->number_r == output2->number_r) ? atom_true : atom_false;
			}
		case OP_LESS_EQUAL:
			if(output1->type == LispType::AtomNumericIntegral) {
				return (output1->number_i <= output2->number_i) ? atom_true : atom_false;
			}
			else {
				return (output1->number_r <= output2->number_r) ? atom_true : atom_false;
			}
		case OP_BIGGER_EQUAL:
			if(output1->type == LispType::AtomNumericIntegral) {
				return (output1->number_i >= output2->number_i) ? atom_true : atom_false;
			}
			else {
				return (output1->number_r >= output2->number_r) ? atom_true : atom_false;
			}
    	case OP_STRING_APPEND:
			if(!output1->is_string() || !output2->is_string()) {
				print_error("string-append", "argument type error\n");

				return nullptr;
			}

			result = new LispNode(AtomString);

			result->string = static_cast<char *>(malloc(strlen(output1->string) + strlen(output2->string) + 1));

			result->string[0] = '\0';
			strcat(result->string, output1->string);
			strcat(result->string, output2->string);

			break;
    	case OP_STRING_REF:
			if(!output1->is_string() || !output2->is_numeric_integral()) {
				print_error("string-ref", "argument type error\n");

				return nullptr;
			}

			if(output2->number_i < 0 || output2->number_i >= strlen(output1->string)) {
				print_error("string-ref", "invalid offset\n");

				return nullptr;
			}

			result = new LispNode(LispType::AtomCharacter);

			result->number_i = output1->string[output2->number_i];

			break;
    	case OP_MAKE_STRING:
			if(!output1->is_numeric() || !output2->is_character()) {
				print_error("make-string", "argument type error\n");

				return nullptr;
			}

			result = new LispNode(LispType::AtomString);

			result->string = static_cast<char *>(malloc(output1->number_i + 1));

			for(size_t i = 0; i < output1->number_i; i++) {
				result->string[i] = static_cast<char>(output2->number_i);
			}

			result->string[output1->number_i] = '\0';

			break;
    	case OP_ASSOC:
			result = make_query_optional_replace(output1, output2);

			if(result == nullptr) {
				return atom_false;
			}

			break;
		default:
			fputs("PANIC: invalid type operator requested\n", stdout);
			exit(EXIT_FAILURE);
	}

	return result;
}

LispNodeRC eval_gen3(LispNodeRC input, LispNodeRC environment) {
	const LispNodeRC &operator_name = input->head->item;

	if(count_members(input) < 4) {
		print_error(operator_name->string, "missing arguments\n");

		return nullptr;
	}

	const LispNodeRC &argument1 = input->head->next->item;
	const LispNodeRC &argument2 = input->head->next->next->item;
	const LispNodeRC &argument3 = input->head->next->next->next->item;

	LispNodeRC output1 = eval_expression(argument1, environment);
	LispNodeRC output2 = eval_expression(argument2, environment);
	LispNodeRC output3 = eval_expression(argument3, environment);
				
	if(output1 == nullptr || output2 == nullptr || output3 == nullptr) {
		return nullptr;
	}

	int operation_index = get_operation_index(operator_name->string);
	LispNodeRC result = nullptr;

	switch(operation_index) {
    	case OP_STRING_SET_E:
			if(!output1->is_string() || !output2->is_numeric_integral() || !output3->is_character()) {
				print_error("string-set!", "argument type error\n");

				return nullptr;
			}

			if(output2->number_i < 0 || output2->number_i >= strlen(output1->string)) {
				print_error("string-set!", "invalid offset\n");

				return nullptr;
			}

			output1->string[output2->number_i] = static_cast<char>(output3->number_i);

			return output1;
    	case OP_SUBSTRING: {
			if(!output1->is_string() || !output2->is_numeric_integral() || !output3->is_numeric_integral()) {
				print_error("substring", "argument type error\n");

				return nullptr;
			}

			if(output2->number_i < 0 || output2->number_i > strlen(output1->string) || output3->number_i < 0 || output3->number_i > strlen(output1->string)) {
				print_error("substring", "invalid offset\n");

				return nullptr;
			}

			char saved = output1->string[output3->number_i];

			// Temporarily changes the output1 string
			output1->string[output3->number_i] = '\0';

			result = new LispNode(LispType::AtomString);
			result->string = strdup(output1->string + output2->number_i);

			// Restore the original character back
			output1->string[output3->number_i] = saved;

			break;
		}
		default:
			fputs("PANIC: invalid type operator requested\n", stdout);
			exit(EXIT_FAILURE);
	}

	return result;
}

LispNodeRC eval_begin(LispNodeRC input, LispNodeRC environment) {
	if(count_members(input) < 2) {
		print_error("begin", "missing arguments\n");

		return nullptr;
	}

	// Setup new context environment
	LispNodeRC new_environment = environment;

	// Indicate to defines the new context environment
	LispNodeRC *old_context_environment = context_environment;
	context_environment = &new_environment;

	LispNodeRC output;

	for(Box *current_expression_box = input->get_head_pointer()->get_next_pointer(); current_expression_box != nullptr; current_expression_box = current_expression_box->get_next_pointer()) {
		if((output = eval_expression(current_expression_box->item, new_environment)) == nullptr) {
			context_environment = old_context_environment;
			return nullptr;
		}
	}

	// The last output is the result of the expression
	context_environment = old_context_environment;
	return output;
}

// Returns the modified environment
LispNodeRC eval_define(LispNodeRC input, LispNodeRC environment) {
	if(count_members(input) != 3) {
		print_error("define/set!", "missing arguments\n");

		return nullptr;
	}

	const LispNodeRC &argument1 = input->head->next->item;
	const LispNodeRC &argument2 = input->head->next->next->item;

	LispNodeRC output1 = argument1->is_atom() ? argument1 : eval_expression(argument1, environment);
	LispNodeRC output2 = eval_expression(argument2, environment);

	if(output1 == nullptr || output2 == nullptr) {
		return nullptr;
	}

	if(!output1->is_atom() || !output1->is_pure()) {
		print_error("define/set!", "argument type error\n");

		return nullptr;
	}

	int operation_index = get_operation_index(input->head->item->string);

	// Side effect: changes the current environment
	if(operation_index == OP_DEFINE) {
		// Just append into environment
		*context_environment = make_cons(make2(output1, output2), *context_environment);
	}
	else if(operation_index == OP_SET_E) {
		// Make query with replacement
		make_query_optional_replace(output1, *context_environment, output2);
	}

	return *context_environment;
}

LispNodeRC eval_eval(LispNodeRC input, LispNodeRC environment) {
	if(count_members(input) != 3) {
		print_error("eval", "missing arguments\n");

		return nullptr;
	}

	const LispNodeRC &argument1 = input->head->next->item;
	const LispNodeRC &argument2 = input->head->next->next->item;

	LispNodeRC output2 = eval_expression(argument2, environment);

	return eval_expression(argument1, output2);
}

LispNodeRC eval_lambda(LispNodeRC input, LispNodeRC environment) {
	if(count_members(input) != 3) {
		print_error("lambda", "missing arguments\n");

		return nullptr;
	}

	const LispNodeRC &argument1 = input->head->next->item;

	if(!argument1->is_list()) {
		print_error("lambda", "argument type error\n");

		return nullptr;
	}

	for(Box *current_parameter_box = argument1->get_head_pointer(); current_parameter_box != nullptr; current_parameter_box = current_parameter_box->get_next_pointer()) {
		if(!current_parameter_box->item->is_atom() || !current_parameter_box->item->is_pure()) {
			print_error("lambda", "argument type error\n");

			return nullptr;
		}
	}

	return input;
}

LispNodeRC eval_lambda_application(LispNodeRC input, LispNodeRC environment) {
	const LispNodeRC &lambda = input->head->item;

	LispNodeRC output = eval_lambda(lambda, environment);

	if(output == nullptr) {
		return nullptr;
	}

	// Defines if we operate on macro substitution mode or in eager evaluation mode
	bool lambda_subst = (get_operation_index(lambda->head->item->string) == OP_MACRO);

	const LispNodeRC &lambda_argument1 = lambda->head->next->item;
	const LispNodeRC &lambda_argument2 = lambda->head->next->next->item;

	// Evaluate the function by setting a new environment for the defined parameters

	// Used when lambda_subst == #t:
	//     Macro expansion: substitutes non-evaluated arguments into parameters in the original expression
	LispNodeRC new_expression = lambda_argument2;
	// Used when lambda_subst == #f:
	//     Eager evaluation: creates a new environment bindin parameters to their eagerly-evaluated arguments
	LispNodeRC new_environment = environment;

	bool packed_dot = false;

	Box *current_parameter_box = lambda_argument1->get_head_pointer();
	Box *current_argument_box = input->get_head_pointer()->get_next_pointer();

	while(current_parameter_box != nullptr || current_argument_box != nullptr) {
		if(current_parameter_box == nullptr) {
			print_error("lambda application", "missing arguments\n");

			return nullptr;
		}

		if(current_argument_box == nullptr) {
			print_error("lambda application", "missing arguments\n");

			return nullptr;
		}

		// Evaluate the argument using the old environment
		LispNodeRC parameter = current_parameter_box->item;
		LispNodeRC argument = current_argument_box->item;

		if(!parameter->is_atom() || !parameter->is_pure()) {
			print_error("lambda application", "argument type error\n");

			return nullptr;
		}

		if(strcmp(parameter->string, ".") == 0) {
			// Get the name of the other parameters and bind them into a list
			current_parameter_box = current_parameter_box->get_next_pointer();
			parameter = current_parameter_box->item;
			
			LispNodeRC list_rest = new LispNode(LispType::List);
			Box *last_box = nullptr;

			while(current_argument_box != nullptr) {
				argument = current_argument_box->item;

				LispNodeRC eval_argument = eval_expression(argument, environment);
				Box *member_box = new Box(eval_argument);

				if(last_box == nullptr) {
					list_rest->head = member_box;
				}
				else {
					last_box->next = member_box;
				}

				last_box = member_box;
				
				current_argument_box = current_argument_box->get_next_pointer();
			}

			LispNodeRC quote = make_operator("quote");
			LispNodeRC quote_list_rest = make2(quote, list_rest);

			argument = quote_list_rest;
			packed_dot = true;
		}

		if(lambda_subst) {
			new_expression = make_substitution(parameter, argument, new_expression);
		}
		else {
			// Resolve the argument in the old environment
			LispNodeRC eval_argument = eval_expression(argument, environment);

			new_environment = make_cons(make2(parameter, eval_argument), new_environment);
		}

		if(packed_dot) {
			break;
		}

		current_parameter_box = current_parameter_box->get_next_pointer();
		current_argument_box = current_argument_box->get_next_pointer();
	}

	// new_expession is only different from expression if macro substitution is done
	// new_environment is only different from environment if normal lambda evaluation is done
	return eval_expression(new_expression, new_environment);
}

LispNodeRC eval_logic2(LispNodeRC input, LispNodeRC environment) {
	const LispNodeRC &operator_name = input->head->item;

	if(count_members(input) != 3) {
		print_error("and/or", "missing arguments\n");

		return nullptr;
	}

	const LispNodeRC &argument1 = input->head->next->item;
	const LispNodeRC &argument2 = input->head->next->next->item;

	LispNodeRC output1 = eval_expression(argument1, environment);

	if(output1 == nullptr) {
		return nullptr;
	}

	if(!output1->is_boolean()) {
		print_error("and/or", "argument type error\n");

		return nullptr;
	}

	int operation_index = get_operation_index(operator_name->string);

	if(operation_index == OP_AND && output1 == atom_false) {
		return atom_false;
	}

	if(operation_index == OP_OR && output1 == atom_true) {
		return atom_true;
	}

	LispNodeRC output2 = eval_expression(argument2, environment);

	if(output2 == nullptr) {
		return nullptr;
	}

	if(!output2->is_boolean()) {
		print_error("and/or", "argument type error\n");

		return nullptr;
	}

	return output2;
}

LispNodeRC eval_subst(LispNodeRC input, LispNodeRC environment) {
	if(count_members(input) != 4) {
		print_error("subst", "missing arguments\n");

		return nullptr;
	}

	const LispNodeRC &argument1 = input->head->next->item;
	const LispNodeRC &argument2 = input->head->next->next->item;
	const LispNodeRC &argument3 = input->head->next->next->next->item;

	LispNodeRC output1 = eval_expression(argument1, environment);
	LispNodeRC output2 = eval_expression(argument2, environment);

	if(output1 == nullptr || output2 == nullptr) {
		return nullptr;
	}

	return make_substitution(output1, output2, argument3);
}

LispNodeRC eval_expression(LispNodeRC input, LispNodeRC environment) {
	if(input->is_atom()) {
		if(input->is_pure()) {
			if(get_operation_index(input->string) != -1) {
				return input;
			}

			LispNodeRC other_input = make_query_optional_replace(input, environment);
			
			if(other_input == nullptr) {
				print_error(input->string, "cannot evaluate\n");

				return nullptr;
			}

			return other_input;
		}

		return input;
	}

	// Here you are certain that input->is_list()

	if(input->head == nullptr) {
		print_error("\'()", "cannot evaluate\n");

		return nullptr;
	}

	const LispNodeRC &first = input->head->item;

	if(first->is_atom() && first->is_pure()) {
		// First try one of the predefined operators
		int operation_index = get_operation_index(first->string);

		switch(operation_index) {
			case OP_QUOTE:
				return eval_quote(input, environment);

			case OP_COND:
				return eval_cond(input, environment);

			case OP_SUBST:
				return eval_subst(input, environment);

			case OP_READ:
			case OP_NEWLINE:
			case OP_CURRENT_ENVIRONMENT:
				return eval_gen0(input, environment);
			
			case OP_CAR:
			case OP_CDR:
			case OP_ATOM_Q:
			case OP_PAIR_Q:
			case OP_CHAR_Q:
			case OP_BOOLEAN_Q:
			case OP_STRING_Q:
			case OP_NUMBER_Q:
			case OP_INTEGER_Q:
			case OP_REAL_Q:
			case OP_INTEGER_REAL:
			case OP_REAL_INTEGER:
			case OP_INTEGER_CHAR:
			case OP_CHAR_INTEGER:
			case OP_NUMBER_STRING:
			case OP_STRING_NUMBER:
			case OP_STRING_LENGTH:
			case OP_NOT:
			case OP_WRITE:
			case OP_DISPLAY:
				return eval_gen1(input, environment);
			
			case OP_EQ_Q:
			case OP_CONS:
			case OP_ASSOC:
			case OP_STRING_APPEND:
			case OP_STRING_REF:
			case OP_MAKE_STRING:
			case OP_PLUS:
			case OP_MINUS:
			case OP_TIMES:
			case OP_DIVIDE:
			case OP_LESS:
			case OP_EQUAL:
			case OP_BIGGER:
			case OP_LESS_EQUAL:
			case OP_BIGGER_EQUAL:
				return eval_gen2(input, environment);

			case OP_STRING_SET_E:
			case OP_SUBSTRING:
				return eval_gen3(input, environment);

			case OP_AND:
			case OP_OR:
				return eval_logic2(input, environment);

			case OP_BEGIN:
				return eval_begin(input, environment);

			case OP_DEFINE:
			case OP_SET_E:
				return eval_define(input, environment);

			case OP_EVAL:
				return eval_eval(input, environment);

			case OP_LAMBDA:
			case OP_MACRO:
				return eval_lambda(input, environment);
		}

		// Evaluate the atom and try again
		LispNodeRC other_first = eval_expression(first, environment);

		if(other_first == nullptr) {
			return nullptr;
		}

		// Change the input to point to the other lambda and evaluaute again
		LispNodeRC parameters = make_cdr(input);
		LispNodeRC other_input = make_cons(other_first, parameters);

		return eval_expression(other_input, environment);
	}

	if(!first->is_list()) {
		return nullptr;
	}

	// Assume it is a lambda application, with function directly or indirectly specified
	return eval_lambda_application(input, environment);
}

void fill_initial_environment() {
	global_environment = list_empty;
	context_environment = &global_environment;

#ifdef INITIAL_ENVIRONMENT
	LispNodeRC parsing_output;

	for(int i = 0; i < NUMBER_INITIAL_LAMBDAS; i++) {
		parsing_output = parse_expression(lambdas[i]);

		eval_expression(parsing_output, global_environment);
	}

	for(int i = 0; i < NUMBER_INITIAL_MACROS; i++) {
		parsing_output = parse_expression(macros[i]);

		eval_expression(parsing_output, global_environment);
	}
#endif /* INITIAL_ENVIROMENT */
}

int main(int argc, char **argv) {
#ifdef TARGET_6502
	#ifdef SIMPLE_ALLOCATOR
		void *lisp_heap = malloc(LISP_HEAP_SIZE);

		allocator.init(lisp_heap, reinterpret_cast<char *>(lisp_heap) + LISP_HEAP_SIZE);
	#else
		__set_heap_limit(LISP_HEAP_SIZE);
	#endif /* SIMPLE_ALLOCATOR */
#endif /* TARGET_6502 */

	// Setup global constants

	atom_true = new LispNode(LispType::AtomBoolean);
	atom_true->string = strdup("#t");

	atom_false = new LispNode(LispType::AtomBoolean);
	atom_false->string = strdup("#f");

	list_empty = new LispNode(LispType::List);
	list_empty->head = nullptr;

	// Setup global environment

	fill_initial_environment();

	// Read-Eval-Print loop

	while(true) {
#ifdef TARGET_6502
		fputs("free: ", stdout);
		print_integral(__heap_bytes_free());
		fputs("\n", stdout);
#endif /* TARGET_6502 */
		fputs("> ", stdout);

		char *input_string = read_expression();

		if(input_string == nullptr) {
			break;
		}

		if(strcmp(input_string, "\n") == 0) {
			continue;
		}

		LispNodeRC input;

		if((input = parse_expression(input_string)) == nullptr) {
			fputs("Error reading expression\n", stdout);
			continue;
		}

		LispNodeRC output;
		context_environment = &global_environment;
		
		if((output = eval_expression(input, global_environment)) == nullptr) {
			fputs("Error evaluating expression\n", stdout);
			continue;
		}

		output->print();

		fputs("\n", stdout);
	}

	fputs("\n", stdout);

	// Frees allocated data from those pointers
	atom_true = nullptr;
	atom_false = nullptr;
	list_empty = nullptr;

	global_environment = nullptr;

#ifdef TARGET_6502
	#ifdef SIMPLE_ALLOCATOR
		free(lisp_heap);
	#endif /* SIMPLE_ALLOCATOR */
#endif /* TARGET_6502 */

	return EXIT_SUCCESS;
}
