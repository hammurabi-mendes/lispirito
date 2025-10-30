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

constexpr unsigned int MAX_EXPRESSION_SIZE = 1024;
constexpr unsigned int MAX_TOKEN_SIZE = 32;

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

// VM stacks used for controlling evaluation and storing results
LispNodeRC evaluation_stack;
LispNodeRC data_stack;

// Forward declarations
char *read_expression();
LispNodeRC parse_expression(char *buffer, bool deallocate_buffer);
LispNodeRC eval_expression(LispNodeRC input, LispNodeRC environment);

#ifdef SIMPLE_ALLOCATOR
	#include "SimpleAllocator.h"

	SimpleAllocator allocator;

	#define Allocate allocator.allocate
	#define Deallocate allocator.deallocate
#else
	#define Allocate malloc
	#define Deallocate free
#endif /* SIMPLE_ALLOCATOR */

void *operator new(size_t size) {
	CounterType *pointer = (CounterType *) Allocate(size + sizeof(CounterType));

	*pointer = 0;

	return pointer + 1;
}

void operator delete(void *pointer) noexcept {
	Deallocate(((CounterType *) pointer) - 1);
}

// Utility functions

void print_error(const char *op, const char *message) {
	fputs(op, stdout);
	fputs(": ", stdout);
	fputs(message, stdout);
}

int string_index(const char *query, const char **list, int maximum) {
	for(int i = 0; i < maximum; i++) {
		if(strcmp(query, list[i]) == 0) {
			return i;
		}
	}

	return -1;
}

#define get_operation_index(query) string_index(query, (const char **) operator_names, NUMBER_BASIC_OPERATORS)
#define get_lambda_index(query) string_index(query, (const char **) lambda_names, NUMBER_INITIAL_LAMBDAS)
#define get_macro_index(query) string_index(query, (const char **) macro_names, NUMBER_INITIAL_MACROS)

// Make operators

LispNodeRC make_operator(char *operator_name) {
	LispNodeRC result = new LispNode(LispType::AtomPure);
	result->data = strdup(operator_name);

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

				value_noconst = replacement.get_pointer();
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

// Parsing

char *read_expression() {
	// Allocated here and deallocated in parse_buffer()
	char *read_buffer = static_cast<char *>(Allocate(MAX_EXPRESSION_SIZE));

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
	static char result[MAX_TOKEN_SIZE];

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

	LispNode *result = new LispNode(LispType::AtomPure);

	// Try first true and false literals

	if(strcmp(token, "#t") == 0) {
		return atom_true;	
	}

	if(strcmp(token, "#f") == 0) {
		return atom_false;	
	}

	if(output & PARSE_CHARACTER) {
		result->type = LispType::AtomCharacter;
		result->number_i = token[2];

		return result;
	}

	if(output & PARSE_QUOTED) {
		// Remove quotes
		token[strlen(token) - 1] = '\0';
		token++;

		result->type = LispType::AtomString;
		result->data = strdup(token);

		return result;
	}

	if(!(output & PARSE_ALPHA) && (output & PARSE_DIGIT) && (output & PARSE_DOT)) {
		result->type = LispType::AtomNumericReal;
		result->number_r = atof(token);

		return result;
	}

	if(!(output & PARSE_ALPHA) && (output & PARSE_DIGIT) && !(output & PARSE_DOT)) {
		result->type = LispType::AtomNumericIntegral;
		result->number_i = atol(token);

		return result;
	}

	// Pure atoms

	result->type = LispType::AtomPure;
	result->data = strdup(token);

	// Converted to LispNodeRC
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

		LispNodeRC quoted = parse_expression(buffer, buffer_length, position, error);

		if(error) {
			return nullptr;
		}

		return make2(make_operator("quote"), quoted);
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

LispNodeRC parse_expression(char *buffer, bool deallocate_buffer = true) {
	// Initial token position
	size_t position = 0;

	// Initial error flag (can change if no token is found)
	bool error = false;

	LispNodeRC result = parse_expression(buffer, strlen(buffer), position, error);

	if(error == true) {
		// Allocated in read_expression() and deallocated here
		if(deallocate_buffer) {
			Deallocate(buffer);
		}

		return nullptr;
	}

	// Allocated in read_expression() and deallocated here
	if(deallocate_buffer) {
		Deallocate(buffer);
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

LispNodeRC eval_gen0(const LispNodeRC &input, const LispNodeRC &environment) {
	const LispNodeRC &operator_name = input->head->item;

	int operation_index = get_operation_index(operator_name->data);

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

LispNodeRC eval_gen1(const LispNodeRC &input, const LispNodeRC &environment) {
	LispNodeRC &operator_name = input->head->item;
	LispNodeRC &output1 = input->head->next->item;

	int operation_index = get_operation_index(operator_name->data);
	LispNode *result = nullptr;

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

			result = new LispNode(LispType::AtomNumericReal);
			result->number_r = output1->number_i;

			break;
		case OP_REAL_INTEGER:
			if(!output1->is_numeric_real()) {
				print_error("real->integer", "argument type error\n");

				return nullptr;
			}

			result = new LispNode(LispType::AtomNumericIntegral);
#ifdef TARGET_6502
			result->number_i = output1->number_r.as_f();
#else
			result->number_i = output1->number_r;
#endif /* TARGET_6502 */

			break;
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

			result = new LispNode(LispType::AtomCharacter);
			result->number_i = output1->number_i;

			break;
    	case OP_CHAR_INTEGER:
			if(!output1->is_character()) {
				print_error("char->integer", "argument type error\n");

				return nullptr;
			}

			result = new LispNode(LispType::AtomNumericIntegral);
			result->number_i = output1->number_i;

			break;
    	case OP_NUMBER_STRING: {
			if(!output1->is_numeric()) {
				print_error("number->string", "argument type error\n");

				return nullptr;
			}

			char *string_buffer = static_cast<char *>(Allocate(MAX_NUMERIC_STRING_LENGTH));

			if(output1->is_numeric_integral()) {
				get_integral_string(output1->number_i, string_buffer);
			}
			else if(output1->is_numeric_real()) {
				get_real_string(output1->number_r, string_buffer);
			}

			result = new LispNode(LispType::AtomString);
			result->data = strdup(string_buffer);

			Deallocate(string_buffer);

			break;
		}
    	case OP_STRING_NUMBER:
			if(!output1->is_string()) {
				print_error("string->number", "argument type error\n");

				return nullptr;
			}

			result = parse_atom(output1->data).get_pointer();

			if(result == nullptr || !result->is_numeric()) {
				print_error("string->number", "argument type error\n");

				return nullptr;
			}

			break;
		case OP_STRING_DATA: {
			if(!output1->is_string()) {
				print_error("string->data", "argument type error\n");

				return nullptr;
			}

			// Note: we do not duplicate data, only convert it in place
			output1->type = LispType::AtomData;
			return output1;
		}
		case OP_DATA_STRING: {
			if(!output1->is_data()) {
				print_error("data->string", "argument type error\n");

				return nullptr;
			}

			// Note: we do not duplicate data, only convert it in place
			output1->type = LispType::AtomString;
			return output1;
		}
    	case OP_DISPLAY:
    	case OP_WRITE:
			output1->print();

			return list_empty;
		case OP_MEM_ALLOC:
			result = new LispNode(LispType::AtomData);
			result->data = static_cast<char *>(Allocate(output1->number_i));

			break;
		case OP_MEM_READ:
			result = new LispNode(LispType::AtomCharacter);
			result->number_i = static_cast<Integral>(*((volatile char *) output1->number_i));

			break;
		case OP_MEM_ADDR:
			result = new LispNode(LispType::AtomNumericIntegral);
			result->number_i = static_cast<Integral>((size_t) output1->data);

			break;
		default:
			fputs("PANIC: invalid type operator requested\n", stdout);
			exit(EXIT_FAILURE);
	}

	return result;
}

LispNodeRC eval_gen2(LispNodeRC &input, const LispNodeRC &environment) {
	LispNodeRC &operator_name = input->head->item;
	LispNodeRC &output1 = input->head->next->item;
	LispNodeRC &output2 = input->head->next->next->item;

	int operation_index = get_operation_index(operator_name->data);
	LispNode *result = nullptr;

	if(operation_index >= OP_PLUS && operation_index <= OP_BIGGER_EQUAL) {
		if(!output1->is_numeric() || !output2->is_numeric()) {
			print_error(operator_name->data, "argument type error\n");

			return nullptr;
		}

		bool promote1 = false;
		bool promote2 = false;

		if(output1->is_numeric_real() && !output2->is_numeric_real()) {
			output2->promoteReal();
			promote2 = true;
		}

		if(!output1->is_numeric_real() && output2->is_numeric_real()) {
			output1->promoteReal();
			promote1 = true;
		}

		if (operation_index >= OP_PLUS && operation_index <= OP_DIVIDE) {
			result = new LispNode(output1->type);

			result->op_arithmetic(operation_index, output1, output2);

			if(promote1) {
				output1->demoteReal();
			}

			if(promote2) {
				output2->demoteReal();
			}

			return result;
		}

		if(operation_index >= OP_LESS && operation_index <= OP_BIGGER_EQUAL) {
			bool comparison_result = output1->op_comparison(operation_index, output2);

			if(promote1) {
				output1->demoteReal();
			}

			if(promote2) {
				output2->demoteReal();
			}

			return (comparison_result ? atom_true : atom_false);
		}
	}

	switch(operation_index) {
		case OP_CONS:
			return make_cons(output1, output2);
		case OP_EQ_Q:
			return (*output1 == *output2) ? atom_true : atom_false;
    	case OP_ASSOC: {
				LispNodeRC replaced = make_query_optional_replace(output1, output2);

				if(replaced == nullptr) {
					return atom_false;
				}

				return replaced;
			}
		case OP_MEM_WRITE: {
			*((volatile char *) output1->number_i) = ((char) output2->number_i);

			return atom_true;
		}
		default:
			fputs("PANIC: invalid type operator requested\n", stdout);
			exit(EXIT_FAILURE);
	}

	return result;
}

LispNodeRC eval_gen3(const LispNodeRC &input, const LispNodeRC &environment) {
	LispNodeRC &operator_name = input->head->item;
	LispNodeRC &output1 = input->head->next->item;
	LispNodeRC &output2 = input->head->next->next->item;
	LispNodeRC &output3 = input->head->next->next->next->item;
	
	int operation_index = get_operation_index(operator_name->data);
	LispNode *result = nullptr;

	switch(operation_index) {
		case OP_SUBST:
			return make_substitution(output1, output2, output3);
		case OP_MEM_FILL: {
			memset((void *) output1->data, (char) output2->number_i, (size_t) output3->number_i);
			return output1;
		}
		case OP_MEM_COPY: {
			memcpy((void *) output1->data, (void *) output2->data, (size_t) output3->number_i);
			return output1;
		}
		default:
			fputs("PANIC: invalid type operator requested\n", stdout);
			exit(EXIT_FAILURE);
	}

	return result;
}

LispNodeRC eval_procedure(const LispNodeRC &input, const LispNodeRC &environment) {
	if(count_members(input) < 3) {
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

LispNodeRC eval_closure(const LispNodeRC &input, const LispNodeRC &environment) {
	return input;
}

LispNodeRC eval_macro(const LispNodeRC &input, const LispNodeRC &environment) {
	return eval_procedure(input, environment);
}

LispNodeRC eval_lambda(const LispNodeRC &input, const LispNodeRC &environment) {
	if(eval_procedure(input, environment) == nullptr) {
		return nullptr;
	}

	return make_cons(make_operator("closure"), make3(input, environment, list_empty));
}

LispNodeRC make_define(const LispNodeRC &operation, const LispNodeRC &symbol, const LispNodeRC &value) {
	if(!symbol->is_atom() || !symbol->is_pure()) {
		print_error("define/set!", "argument type error\n");

		return nullptr;
	}

	int operation_index = get_operation_index(operation->data);

	// Side effect: changes the current environment
	if(operation_index == OP_DEFINE) {
		// Just append into environment
		*context_environment = make_cons(make2(symbol, value), *context_environment);
	}
	else if(operation_index == OP_SET_E) {
		// Make query with replacement
		make_query_optional_replace(symbol, *context_environment, value);
	}

	// Make closures' environments to include their own definition (for recursion)
	if(value->is_operator("closure")) {
		LispNodeRC &procedure_name = value->head->next->next->next->item;

		procedure_name = symbol;
	}

	return *context_environment;
}

LispNodeRC make_lambda_application(const LispNodeRC &input, const LispNodeRC &environment) {
	const LispNodeRC &procedure_or_closure = input->head->item;

	int operator_index = get_operation_index(procedure_or_closure->head->item->data);

	if(operator_index != OP_CLOSURE && operator_index != OP_LAMBDA && operator_index != OP_MACRO) {
		return nullptr;
	}

	// Defines if we operate on macro substitution mode or in lambda/closure evaluation mode
	bool lambda_subst = (operator_index == OP_MACRO);

	const LispNodeRC &procedure = (operator_index == OP_CLOSURE ? procedure_or_closure->head->next->item : procedure_or_closure);

	const LispNodeRC &procedure_parameters = procedure->head->next->item;

	// Evaluate the function by setting a new environment for the defined parameters

	// Used when lambda_subst == #t:
	//     Macro expansion: substitutes non-evaluated parameters into arguments in the original expression
	LispNodeRC new_expression = LispNode::make_list(procedure->get_head_pointer()->get_next_pointer()->get_next_pointer());
	// Used when lambda_subst == #f:
	//     Eager evaluation: creates a new environment binding parameters to their eagerly-evaluated arguments
	LispNodeRC new_environment = (operator_index == OP_CLOSURE ? procedure_or_closure->head->next->next->item : environment);

	if(operator_index == OP_CLOSURE) {
		const LispNodeRC &procedure_name = procedure_or_closure->head->next->next->next->item;

		if(procedure_name != list_empty) {
			new_environment = make_cons(make2(procedure_name, procedure), new_environment);
		}
	}

	bool packed_dot = false;

	Box *current_parameter_box = procedure_parameters->get_head_pointer();
	Box *current_argument_box = input->get_head_pointer()->get_next_pointer();

	while(current_parameter_box != nullptr || current_argument_box != nullptr) {
		if(current_parameter_box == nullptr || current_argument_box == nullptr) {
			print_error("lambda application", "missing or extra arguments\n");

			return nullptr;
		}

		// Evaluate the argument using the old environment
		LispNodeRC parameter = current_parameter_box->item;
		LispNodeRC argument = current_argument_box->item;

		if(!parameter->is_atom() || !parameter->is_pure()) {
			print_error("lambda application", "argument type error\n");

			return nullptr;
		}

		if(strcmp(parameter->data, ".") == 0) {
			// Get the name of the other parameters and bind them into a list
			current_parameter_box = current_parameter_box->get_next_pointer();
			parameter = current_parameter_box->item;
			
			LispNodeRC list_rest = LispNode::make_list(current_argument_box);
			LispNodeRC quote_list_rest = make2(make_operator("quote"), list_rest);

			argument = quote_list_rest;
			packed_dot = true;
		}

		if(lambda_subst) {
			new_expression = make_substitution(parameter, argument, new_expression);
		}
		else {
			// Note that argument has been already evaluated in the old environment
			new_environment = make_cons(make2(parameter, argument), new_environment);
		}

		if(packed_dot) {
			break;
		}

		current_parameter_box = current_parameter_box->get_next_pointer();
		current_argument_box = current_argument_box->get_next_pointer();
	}

	// new_expression is only different from expression if macro substitution is done
	// new_environment is only different from environment if lambda evaluation is done
	return make2(new_expression, new_environment);
}

LispNodeRC make_environment() {
	LispNodeRC result = *context_environment;

	return result;
}

// VM functions

void vm_push(const LispNodeRC &node) {
    evaluation_stack = make_cons(node, evaluation_stack);
}

const LispNodeRC &vm_peek() {
	if(evaluation_stack != list_empty) {
		return evaluation_stack->head->item;
	}

	return list_empty;
}

void data_push(const LispNodeRC &node) {
    data_stack = make_cons(node, data_stack);
}

LispNodeRC &data_peek() {
	if(data_stack != list_empty) {
		return data_stack->head->item;
	}

	return list_empty;
}

LispNodeRC data_pop() {
	LispNodeRC result = data_peek();

    data_stack = make_cdr(data_stack);

	return result;
}

void vm_reset() {
	evaluation_stack = list_empty;
	data_stack = list_empty;
}

void vm_finish() {
	evaluation_stack = list_empty;
}

void eval_reduce(const LispNodeRC &input, const LispNodeRC &environment) {
	if(input->is_atom()) {
		if(input->is_pure()) {
			// Try to get an environment definition

			LispNodeRC other_input = make_query_optional_replace(input, environment);
			
			if(other_input != nullptr) {
				data_push(other_input);
				return;
			}

			// Try to get a basic operator

			if(get_operation_index(input->data) != -1) {
				data_push(input);
				return;
			}

			// All attemps failed at this point

			print_error(input->data, "cannot evaluate\n");
			vm_finish();

			return;
		}

		data_push(input);
		return;
	}

	// Input is a list...

	if(input->head == nullptr) {
		print_error("\'()", "cannot evaluate\n");
		vm_finish();

		return;
	}

	const LispNodeRC &first = input->head->item;

	if(first->is_atom() && first->is_pure()) {
		// First try one of the predefined operators
		int operation_index = get_operation_index(first->data);

		switch(operation_index) {
			case OP_QUOTE:
				// Special: does not evaluate
				vm_push(make3(make_operator("vm-quote"), list_empty, make2(input, environment)));
				return;

			case OP_COND:
				// Special:
				vm_push(make3(make_operator("vm-cond"), atom_false, make2(make_cdr(input), environment)));
				return;

			case OP_READ:
			case OP_NEWLINE:
			case OP_CURRENT_ENVIRONMENT:
				// Normal:
				vm_push(make3(make_operator("vm-normal"), LispNode::make_integer(0), make2(input, environment)));
				return;

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
			case OP_STRING_DATA:
			case OP_DATA_STRING:
			case OP_NOT:
			case OP_WRITE:
			case OP_DISPLAY:
			case OP_MEM_ALLOC:
			case OP_MEM_READ:
			case OP_MEM_ADDR:
				// Normal:
				vm_push(make3(make_operator("vm-normal"), LispNode::make_integer(1), make2(input, environment)));
				return;

			case OP_LOAD:
			case OP_UNLOAD:
				// Special:
				vm_push(make3(make_operator("vm-load"), make2(LispNode::make_integer(operation_index), atom_false), make2(input, environment)));
				return;

			case OP_EQ_Q:
			case OP_CONS:
			case OP_ASSOC:
			case OP_PLUS:
			case OP_MINUS:
			case OP_TIMES:
			case OP_DIVIDE:
			case OP_LESS:
			case OP_EQUAL:
			case OP_BIGGER:
			case OP_LESS_EQUAL:
			case OP_BIGGER_EQUAL:
			case OP_MEM_WRITE:
				// Normal:
				vm_push(make3(make_operator("vm-normal"), LispNode::make_integer(2), make2(input, environment)));
				return;

			case OP_SUBST:
			case OP_MEM_FILL:
			case OP_MEM_COPY:
				// Normal:
				vm_push(make3(make_operator("vm-normal"), LispNode::make_integer(3), make2(input, environment)));
				return;

			case OP_AND:
			case OP_OR:
				// Special:
				vm_push(make3(make_operator("vm-logic"), make2(LispNode::make_integer(operation_index), atom_false), make2(make_cdr(input), environment)));
				return;

			case OP_BEGIN:
				// Special:
				vm_push(make3(make_operator("vm-begin"), make2(environment, atom_false), make2(make_cdr(input), make_environment())));
				return;

			case OP_DEFINE:
			case OP_SET_E:
				// Special:
				vm_push(make3(make_operator("vm-define"), atom_false, make2(input, environment)));
				return;

			case OP_EVAL:
				vm_push(make2(input, environment));
				return;

			case OP_MACRO:
				data_push(eval_macro(input, environment));
				return;

			case OP_CLOSURE:
				data_push(eval_closure(input, environment));
				return;

			case OP_LAMBDA:
				data_push(eval_lambda(input, environment));
				return;
		}
	}

	// Here, the operator is itself a list
	// 		1) If it has been evaluated, it should be a closure or a macro
	//		2) If it has not been evaluated, we create a new ("vm-first" ...)

	if(first->is_operator("closure") || first->is_operator("macro")) {
		vm_push(make3(make_operator("vm-apply"), make2(LispNode::make_integer(count_members(input) - 1), atom_false), make2(input, environment)));
		return;
	}
	else {
		vm_push(make3(make_operator("vm-first"), atom_false, make2(input, environment)));
		return;
	}

	print_error("eval_reduce()", "unknown form");
	input->print();
	return;
}

void vm_step() {
	// Create another reference to the top of the evaluation stack,
	// so the data it points to is still valid after we pop from it
	LispNodeRC top = vm_peek();

	const LispNodeRC &vm_op_name = top->head->item;
	LispNodeRC &vm_op_state = top->head->next->item;
	const LispNodeRC &vm_op_arguments = top->head->next->next->item;

	const LispNodeRC &input = vm_op_arguments->head->item;
	const LispNodeRC &environment = vm_op_arguments->head->next->item;

	int operation_index = get_operation_index(vm_op_name->data);

	switch(operation_index) {
		// (vm-first <waiting> (input environment))
		case OP_VM_FIRST: {
			LispNodeRC &waiting = vm_op_state;

			if(waiting == atom_false) {
				vm_push(make3(make_operator("vm-eval"), list_empty, make2(input->head->item, environment)));
				waiting = atom_true;
			}
			else {
				LispNodeRC result = data_pop();

				evaluation_stack = make_cdr(evaluation_stack);

				vm_push(make3(make_operator("vm-eval"), list_empty, make2(make_cons(result, make_cdr(input)), environment)));
			}

			return;
		}
		// (vm-normal <arity> (input environment))
		case OP_VM_NORMAL: {
			LispNodeRC &arity = vm_op_state;

			if(count_members(input) != arity->number_i + 1) {
				print_error(input->head->item->data, "missing arguments\n");
				vm_finish();

				return;
			}

			evaluation_stack = make_cdr(evaluation_stack);

			vm_push(make3(make_operator("vm-call"), vm_op_state, vm_op_arguments));
			vm_push(make3(make_operator("vm-eval-list"), make2(atom_false, atom_false), make2(make_cdr(input), environment)));

			return;
		}
		// (vm-quote () (input environment))
		case OP_VM_QUOTE: {
			if(count_members(input) != 2) {
				print_error(input->head->item->data, "missing arguments\n");
				vm_finish();

				return;
			}

			const LispNodeRC &argument = input->head->next->item;

			evaluation_stack = make_cdr(evaluation_stack);
			data_push(argument);

			return;
		}
		// (vm-cond <waiting> ([(t1 c1) ... (tN cN)] environment))
		case OP_VM_COND: {
			LispNodeRC &waiting = vm_op_state;

			LispNodeRC &evaluation_pairs = vm_op_arguments->head->item;
			const LispNodeRC &environment = vm_op_arguments->head->next->item;

			if(waiting == atom_false && evaluation_pairs == list_empty) {
				evaluation_stack = make_cdr(evaluation_stack);

				data_push(list_empty);

				return;
			}

			const LispNodeRC &current_pair = evaluation_pairs->head->item;

			const LispNodeRC &current_test = current_pair->head->item;
			const LispNodeRC &current_consequent = current_pair->head->next->item;

			if(waiting == atom_false) {
				vm_push(make3(make_operator("vm-eval"), list_empty, make2(current_test, environment)));
				vm_op_state = atom_true;
			}
			else {
				LispNodeRC result = data_pop();

				if(result == atom_true) {
					evaluation_stack = make_cdr(evaluation_stack);
					vm_push(make3(make_operator("vm-eval"), list_empty, make2(current_consequent, environment)));

					return;
				}

				evaluation_pairs = make_cdr(evaluation_pairs);
				waiting = atom_false;
			}

			return;
		}
		// (vm-logic (<OP_AND/OP_OR> <waiting>) (evaluation_items environment))
		case OP_VM_LOGIC: {
			const LispNodeRC &type = vm_op_state->head->item;
			LispNodeRC &waiting = vm_op_state->head->next->item;

			LispNodeRC &evaluation_items = vm_op_arguments->head->item;
			const LispNodeRC &environment = vm_op_arguments->head->next->item;

			if(waiting == atom_false) {
				if(evaluation_items == list_empty) {
					evaluation_stack = make_cdr(evaluation_stack);

					if(type->number_i == OP_AND) {
						data_push(atom_true);
					}

					if(type->number_i == OP_OR) {
						data_push(atom_false);
					}

					return;
				}

				vm_push(make3(make_operator("vm-eval"), list_empty, make2(evaluation_items->head->item, environment)));
				waiting = atom_true;
			}
			else {
				LispNodeRC result = data_pop();

				if(type->number_i == OP_AND && result == atom_false) {
					evaluation_stack = make_cdr(evaluation_stack);
					data_push(atom_false);

					return;
				}
				if(type->number_i == OP_OR && result == atom_true) {
					evaluation_stack = make_cdr(evaluation_stack);
					data_push(atom_true);

					return;
				}

				evaluation_items = make_cdr(evaluation_items);
				waiting = atom_false;
			}

			return;
		}
		// (vm-define <waiting> (input environment))
		case OP_VM_DEFINE: {
			LispNodeRC &waiting = vm_op_state;

			const LispNodeRC &input = vm_op_arguments->head->item;
			const LispNodeRC &environment = vm_op_arguments->head->next->item;

			const LispNodeRC &operation = input->head->item;
			const LispNodeRC &symbol = input->head->next->item;

			if(waiting == atom_false) {
				LispNodeRC expression = input->head->next->next->item;

				if(symbol->is_list()) {
					expression = make_cons(make_operator("lambda"), make_cons(make_cdr(symbol), LispNode::make_list(input->get_head_pointer()->get_next_pointer()->get_next_pointer())));
				}

				vm_push(make3(make_operator("vm-eval"), list_empty, make2(expression, environment)));
				waiting = atom_true;
			}
			else {
				const LispNodeRC &operation = input->head->item;
				const LispNodeRC &symbol = input->head->next->item;

				LispNodeRC result = data_pop();

				if(symbol->is_list()) {
					make_define(operation, symbol->head->item, result);
				}
				else {
					make_define(operation, symbol, result);
				}

				evaluation_stack = make_cdr(evaluation_stack);
				data_push(list_empty);
			}

			return;
		}
		// (vm-begin (<old_environment> <waiting>) (evaluation_items environment))
		case OP_VM_BEGIN: {
			const LispNodeRC &old_environment = vm_op_state->head->item;
			LispNodeRC &waiting = vm_op_state->head->next->item;

			if(waiting == atom_false) {
				*context_environment = environment;

				vm_push(make3(make_operator("vm-eval-list"), make2(atom_false, atom_false), vm_op_arguments));
				waiting = atom_true;
			}
			else {
				*context_environment = old_environment;

				evaluation_stack = make_cdr(evaluation_stack);
			}

			return;
		}
		// (vm-apply (<arity> <waiting>) (input environment))
		case OP_VM_APPLY: {
			const LispNodeRC &arity = vm_op_state->head->item;
			LispNodeRC &waiting = vm_op_state->head->next->item;

			if(waiting == atom_false) {
				vm_push(make3(make_operator("vm-eval-list"), make2(atom_false, atom_false), make2(make_cdr(input), environment)));
				waiting = atom_true;
			}
			else {
				LispNodeRC evaluated_input = list_empty;

				for(size_t i = 0; i < arity->number_i; i++) {
					evaluated_input = make_cons(data_pop(), evaluated_input);
				}

				// Add the original operator to the front of the evaluated arguments
				evaluated_input = make_cons(input->head->item, evaluated_input);

				LispNodeRC lambda_application = make_lambda_application(evaluated_input, environment);

				const LispNodeRC &new_expression = lambda_application->head->item;
				const LispNodeRC &new_environment = lambda_application->head->next->item;

				evaluation_stack = make_cdr(evaluation_stack);
				vm_push(make3(make_operator("vm-begin"), make2(environment, atom_false), make2(new_expression, new_environment)));
			}

			return;
		}
		// (vm-eval '() (input environment))
		case OP_VM_EVAL: {
			evaluation_stack = make_cdr(evaluation_stack);
			eval_reduce(input, environment);

			return;
		}
		// (vm-load (<type>, <waiting>) (input environment))
		case OP_VM_LOAD: {
			const LispNodeRC &type = vm_op_state->head->item;
			LispNodeRC &waiting = vm_op_state->head->next->item;

			if(waiting == atom_false) {
				const LispNodeRC &symbol = input->head->next->item;

				vm_push(make3(make_operator("vm-eval"), list_empty, make2(symbol, environment)));

				waiting = atom_true;
			}
			else {
				LispNodeRC evaluated_symbol = data_pop();

				if(type->number_i == OP_LOAD) {
#ifdef INITIAL_ENVIRONMENT
					int index;
					char *value = nullptr;

					if((index = get_lambda_index(evaluated_symbol->data)) != -1) {
						value = lambda_strings[index];
					}

					if((index = get_macro_index(evaluated_symbol->data)) != -1) {
						value = macro_strings[index];
					}

					LispNodeRC load_expression = make3(make_operator("define"), evaluated_symbol, parse_expression(value, false));

					evaluation_stack = make_cdr(evaluation_stack);
					vm_push(make3(make_operator("vm-eval"), list_empty, make2(load_expression, environment)));
#else
					print_error("load", "no compiled support\n");
					vm_finish();
#endif /* INITIAL_ENVIRONMENT */
				}
				else {
					LispNodeRC unload_expression = make3(make_operator("set!"), evaluated_symbol, atom_false);

					evaluation_stack = make_cdr(evaluation_stack);
					vm_push(make3(make_operator("vm-eval"), list_empty, make2(input, environment)));
				}
			}

			return;
		}
		// (vm-call <arity> (input environment))
		case OP_VM_CALL: {
			LispNodeRC &arity = vm_op_state;

			LispNodeRC evaluated_input = list_empty;

			for(size_t i = 0; i < arity->number_i; i++) {
				evaluated_input = make_cons(data_pop(), evaluated_input);
			}
				
			// Add the original operator to the front of the evaluated arguments
			evaluated_input = make_cons(input->head->item, evaluated_input);

			LispNodeRC result;

			switch(arity->number_i) {
				case 0:
					result = eval_gen0(evaluated_input, environment);
					break;
				case 1:
					result = eval_gen1(evaluated_input, environment);
					break;
				case 2:
					result = eval_gen2(evaluated_input, environment);
					break;
				case 3:
					result = eval_gen3(evaluated_input, environment);
					break;
			}

			if(result == nullptr) {
				// Error message printed in the eval_genX function
				vm_finish();

				return;
			}

			evaluation_stack = make_cdr(evaluation_stack);
			data_push(result);

			return;
		}
		// (vm-eval-list (<discard-itermediary> <waiting>) (evaluation_items environment))
		case OP_VM_EVAL_LIST: {
			const LispNodeRC &discard_intermediary = vm_op_state->head->item;
			LispNodeRC &waiting = vm_op_state->head->next->item;

			LispNodeRC &evaluation_items = vm_op_arguments->head->item;
			const LispNodeRC &environment = vm_op_arguments->head->next->item;

			if(evaluation_items != list_empty) {
				if(waiting == atom_true && discard_intermediary == atom_true) {
					data_pop();
				}

				vm_push(make3(make_operator("vm-eval"), list_empty, make2(make_car(evaluation_items), environment)));
				evaluation_items = make_cdr(evaluation_items);

				waiting = atom_true;
			}
			else {
				evaluation_stack = make_cdr(evaluation_stack);
			}

			return;
		}
		default:
			print_error("vm_step()", "unknown operation");
			vm_finish();
	}
}

LispNodeRC eval_expression(const LispNodeRC input, const LispNodeRC environment) {
	vm_reset();
	vm_push(make3(make_operator("vm-eval"), list_empty, make2(input, environment)));

	while(evaluation_stack != list_empty) {
		vm_step();
	}

	return data_peek();
}

int main(int argc, char **argv) {
#ifdef TARGET_6502
	__set_heap_limit(LISP_HEAP_SIZE);
#endif /* TARGET_6502 */

	// Setup global constants

	atom_true = new LispNode(LispType::AtomBoolean);
	atom_true->data = strdup("#t");

	atom_false = new LispNode(LispType::AtomBoolean);
	atom_false->data = strdup("#f");

	list_empty = new LispNode(LispType::List);
	list_empty->head = nullptr;

	// Setup global environment

	global_environment = list_empty;

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

	return EXIT_SUCCESS;
}
