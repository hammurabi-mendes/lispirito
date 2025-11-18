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

LispNodeRC make_operator(int operation_index) {
	LispNode *result = new LispNode(LispType::AtomOperator);
	result->number_i = operation_index;

	return result;
}

LispNodeRC make1(const LispNodeRC &first) {
	LispNode *result = new LispNode(LispType::List);

	result->head = new Box(first);

	return result;
}

LispNodeRC make2(const LispNodeRC &first, const LispNodeRC &second) {
	LispNode *result = new LispNode(LispType::List);

	result->head = new Box(first);
	result->head->next = new Box(second);

	return result;
}

LispNodeRC make3(const LispNodeRC &first, const LispNodeRC &second, const LispNodeRC &third) {
	LispNode *result = new LispNode(LispType::List);

	result->head = new Box(first);
	result->head->next = new Box(second);
	result->head->next->next = new Box(third);

	return result;
}

LispNodeRC make_cons(const LispNodeRC &first, const LispNodeRC &second) {
	LispNode *result = new LispNode(LispType::List);

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
	if(!list->is_list() || list->head == nullptr) {
		return nullptr;
	}

	return list->head->item;
}

LispNodeRC make_cdr(const LispNodeRC &list) {
	if(!list->is_list() || list->head == nullptr) {
		return nullptr;
	}

	Box *second_element_box = list->get_head_pointer()->get_next_pointer();

	if(second_element_box == nullptr) {
		return list_empty;
	}

	LispNode *result = LispNode::make_list(second_element_box);

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

	LispNode *output = new LispNode(LispType::List);

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
		Deallocate(read_buffer);
		read_buffer = nullptr;
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

	// Operator or pure atoms

	int operation_index = get_operation_index(token);

	if(operation_index != -1) {
		result->type = LispType::AtomOperator;
		result->number_i = operation_index;
	}
	else {
		result->type = LispType::AtomPure;
		result->data = strdup(token);
	}

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

		return make2(make_operator(OP_QUOTE), quoted);
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

	// Allocated in read_expression() and deallocated here
	if(deallocate_buffer) {
		Deallocate(buffer);
	}

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

LispNodeRC eval_gen0(const LispNodeRC &input, const LispNodeRC &environment) {
	const LispNodeRC &operator_name = input->head->item;

	int operation_index = operator_name->number_i;

	switch(operation_index) {
		case OP_READ: {
			char *input_string = read_expression();

			if(input_string == nullptr) {
				break;
			}

			return parse_expression(input_string);
		}
    	case OP_NEWLINE: {
			fputs("\n", stdout);

			return list_empty;
		}
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

	int operation_index = operator_name->number_i;
	LispNode *result = nullptr;

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
				return nullptr;
			}

			result = new LispNode(LispType::AtomNumericReal);
			result->number_r = output1->number_i;

			break;
		case OP_REAL_INTEGER:
			if(!output1->is_numeric_real()) {
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
				return nullptr;
			}

			return (output1 == atom_true ? atom_false : atom_true);
		case OP_INTEGER_CHAR:
			if(!output1->is_numeric_integral()) {
				return nullptr;
			}

			result = new LispNode(LispType::AtomCharacter);
			result->number_i = output1->number_i;

			break;
    	case OP_CHAR_INTEGER:
			if(!output1->is_character()) {
				return nullptr;
			}

			result = new LispNode(LispType::AtomNumericIntegral);
			result->number_i = output1->number_i;

			break;
    	case OP_NUMBER_STRING: {
			if(!output1->is_numeric()) {
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
				return nullptr;
			}

			result = parse_atom(output1->data).get_pointer();

			if(result == nullptr || !result->is_numeric()) {
				return nullptr;
			}

			break;
		case OP_STRING_DATA: {
			if(!output1->is_string()) {
				return nullptr;
			}

			// Note: we do not duplicate data, only convert it in place
			output1->type = LispType::AtomData;
			return output1;
		}
		case OP_DATA_STRING: {
			if(!output1->is_data()) {
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
			result->data = static_cast<char *>(malloc(output1->number_i));

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

	int operation_index = operator_name->number_i;
	LispNode *result = nullptr;

	if(operation_index >= OP_PLUS && operation_index <= OP_BIGGER_EQUAL) {
		if(!output1->is_numeric() || !output2->is_numeric()) {
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
			if(operation_index == OP_DIVIDE) {
				if((output2->is_numeric_integral() && output2->number_i == 0) || (output2->is_numeric_real() && output2->number_r == Real(0.0))) {
					return nullptr;
				}
			}

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
	
	int operation_index = operator_name->number_i;
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

	return make_cons(make_operator(OP_CLOSURE), make2(input, environment));
}

LispNodeRC make_environment(const LispNodeRC &environment) {
	// Creates an environment with a dummy entry to separate frames
	LispNodeRC new_environment = make_cons(make2(atom_false, atom_false), environment);

	return new_environment;
}

LispNodeRC make_lambda_application(const LispNodeRC &input, const LispNodeRC &environment) {
	const LispNodeRC &closure_or_macro = input->head->item;

	int operator_index = closure_or_macro->head->item->number_i;

	// Defines if we operate on macro substitution mode or in closure application mode
	bool is_closure = (operator_index == OP_CLOSURE);
	bool is_macro = (operator_index == OP_MACRO);

	const LispNodeRC &procedure = (is_closure ? closure_or_macro->head->next->item : closure_or_macro);
	const LispNodeRC &procedure_parameters = procedure->head->next->item;

	// Evaluate the function by setting a new environment for the defined parameters

	// Used when is_macro == #t:
	//     Macro expansion: substitutes non-evaluated parameters into arguments in the original expression
	LispNodeRC new_expression = LispNode::make_list(procedure->get_head_pointer()->get_next_pointer()->get_next_pointer());
	// Used when is_macro == #f:
	//     Eager evaluation: creates a new environment binding parameters to their eagerly-evaluated arguments
	LispNodeRC new_environment = (operator_index == OP_CLOSURE ? make_environment(closure_or_macro->head->next->next->item) : environment);

	bool packed_dot = false;

	Box *current_parameter_box = procedure_parameters->get_head_pointer();
	Box *current_argument_box = input->get_head_pointer()->get_next_pointer();

	while(current_parameter_box != nullptr || current_argument_box != nullptr) {
		if(current_parameter_box == nullptr || current_argument_box == nullptr) {
			print_error("operator application", "missing or extra arguments\n");

			return nullptr;
		}

		// Evaluate the argument using the old environment
		LispNodeRC parameter = current_parameter_box->item;
		LispNodeRC argument = current_argument_box->item;

		if(!parameter->is_atom() || !parameter->is_pure()) {
			print_error("operator application", "argument type error\n");

			return nullptr;
		}

		if(strcmp(parameter->data, ".") == 0) {
			// Get the name of the other parameters and bind them into a list
			current_parameter_box = current_parameter_box->get_next_pointer();
			parameter = current_parameter_box->item;
			
			argument = LispNode::make_list(current_argument_box);
			packed_dot = true;
		}

		if(is_macro) {
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
	// new_environment is only different from environment if closure application is done
	return make2(new_expression, new_environment);
}

// VM functions

inline void vm_push(const LispNodeRC &node) {
    evaluation_stack = make_cons(node, evaluation_stack);
}

inline const LispNodeRC &vm_peek() {
	return evaluation_stack->head->item;
}

inline void vm_pop() {
	evaluation_stack = make_cdr(evaluation_stack);
}

inline void data_push(const LispNodeRC &node) {
    data_stack = make_cons(node, data_stack);
}

inline LispNodeRC &data_peek() {
	return data_stack->head->item;
}

inline void data_pop() {
    data_stack = make_cdr(data_stack);
}

// I want to differentiate reset and finish, but for now they work the same way
#define vm_finish vm_reset

void vm_reset() {
	evaluation_stack = list_empty;
	data_stack = list_empty;

	context_environment = &global_environment;
}

bool eval_reduce(const LispNodeRC &input, const LispNodeRC &environment) {
	if(input->is_atom()) {
		if(input->is_pure()) {
			// Try to get an environment definition

			LispNodeRC other_input = make_query_optional_replace(input, environment);
			
			if(other_input != nullptr) {
				data_push(other_input);
				return true;
			}

			print_error(input->data, "evaluation error\n");
			return false;
		}

		data_push(input);
		return true;
	}

	// Input is a list...

	if(input->head == nullptr) {
		print_error("\'()", "evaluation error\n");
		return false;
	}

	const LispNodeRC &first = input->head->item;

	if(first->is_operator()) {
		// First try one of the predefined operators
		int operation_index = first->number_i;
		int operation_reduce_mode = operation_index >= 0 ? operator_reduce_modes[first->number_i] : Unspecified;

		switch(operation_reduce_mode) {
			case SpecialQuote:
				// Special: does not evaluate
				vm_push(make3(make_operator(OP_VM_QUOTE), list_empty, make2(input, environment)));
				return true;

			case SpecialCond:
				// Special:
				vm_push(make3(make_operator(OP_VM_COND), atom_false, make2(make_cdr(input), environment)));
				return true;

			case Normal0:
				// Normal:
				vm_push(make3(make_operator(OP_VM_NORMAL), LispNode::make_integer(0), make2(input, environment)));
				return true;

			case Normal1:
				// Normal:
				vm_push(make3(make_operator(OP_VM_NORMAL), LispNode::make_integer(1), make2(input, environment)));
				return true;

			case SpecialLoad:
				// Special:
				vm_push(make3(make_operator(OP_VM_LOAD), make2(LispNode::make_integer(operation_index), atom_false), make2(input, environment)));
				return true;

			case Normal2:
				// Normal:
				vm_push(make3(make_operator(OP_VM_NORMAL), LispNode::make_integer(2), make2(input, environment)));
				return true;

			case Normal3:
				// Normal:
				vm_push(make3(make_operator(OP_VM_NORMAL), LispNode::make_integer(3), make2(input, environment)));
				return true;

			case SpecialLogic:
				// Special:
				vm_push(make3(make_operator(OP_VM_LOGIC), make2(LispNode::make_integer(operation_index), atom_false), make2(make_cdr(input), environment)));
				return true;

			case SpecialBegin:
				// Special:
				vm_push(make3(make_operator(OP_VM_BEGIN), make3(list_empty, list_empty, atom_false), make2(make_cdr(input), make_environment(environment))));
				return true;

			case SpecialDefine:
				// Special:
				vm_push(make3(make_operator(OP_VM_DEFINE), make2(LispNode::make_integer(operation_index), atom_false), make2(input, environment)));
				return true;


			case SpecialEval:
				vm_push(make3(make_operator(OP_VM_EVAL), list_empty, make2(input, environment)));
				return true;

			case ImmediateLambda:
				data_push(eval_lambda(input, environment));
				return true;

			case ImmediateMacro:
				data_push(eval_macro(input, environment));
				return true;

			case ImmediateClosure:
				data_push(eval_closure(input, environment));
				return true;

			case NormalX:
				// Normal:
				vm_push(make3(make_operator(OP_VM_NORMAL), LispNode::make_integer(count_members(input) - 1), make2(input, environment)));
				return true;
		}
	}

	// Here, the operator is itself a list
	// 		1) If it has been evaluated, it should be a closure or a macro
	//		2) If it has not been evaluated, we create a new ("vm-first" ...)

	bool is_closure = first->is_operation(OP_CLOSURE);
	bool is_macro = first->is_operation(OP_MACRO);

	if(is_closure || is_macro) {
		vm_push(make3(make_operator(OP_VM_APPLY), make3(LispNode::make_integer(count_members(input) - 1), is_closure ? atom_true : atom_false, atom_false), make2(input, environment)));
		return true;
	}

	if((first->is_atom() && first->is_pure()) || first->is_list()) {
		vm_push(make3(make_operator(OP_VM_FIRST), atom_false, make2(input, environment)));
		return true;
	}

	input->print();
	print_error(" at eval_reduce()", "unknown form\n");

	return false;
}

void vm_step() {
	// Create another reference to the top of the evaluation stack,
	// so the data it points to is still valid after we pop from it
	LispNodeRC top = vm_peek();

	const LispNodeRC &vm_op = top->head->item;
	LispNodeRC &vm_op_state = top->head->next->item;
	const LispNodeRC &vm_op_arguments = top->head->next->next->item;

	const LispNodeRC &input = vm_op_arguments->head->item;
	const LispNodeRC &environment = vm_op_arguments->head->next->item;

	int operation_index = vm_op->number_i;

	switch(operation_index) {
		// (vm-first <waiting> (input environment))
		case OP_VM_FIRST: {
			LispNodeRC &waiting = vm_op_state;

			if(waiting == atom_false) {
				vm_push(make3(make_operator(OP_VM_EVAL), list_empty, make2(input->head->item, environment)));
				waiting = atom_true;
			}
			else {
				LispNodeRC result = data_peek();
				data_pop();

				vm_pop();
				vm_push(make3(make_operator(OP_VM_EVAL), list_empty, make2(make_cons(result, make_cdr(input)), environment)));
			}

			return;
		}
		// (vm-normal <arity> (input environment))
		case OP_VM_NORMAL: {
			LispNodeRC &arity = vm_op_state;

			if(count_members(input) != arity->number_i + 1) {
				print_error(input->head->item->data, "missing or extra arguments\n");
				vm_finish();

				return;
			}

			vm_pop();

			vm_push(make3(make_operator(OP_VM_CALL), vm_op_state, vm_op_arguments));
			vm_push(make3(make_operator(OP_VM_EVAL_LIST), make2(atom_false, atom_false), make2(make_cdr(input), environment)));

			return;
		}
		// (vm-quote () (input environment))
		case OP_VM_QUOTE: {
			if(count_members(input) != 2) {
				print_error(input->head->item->data, "missing or extra arguments\n");
				vm_finish();

				return;
			}

			const LispNodeRC &quoted_expression = input->head->next->item;

			vm_pop();
			data_push(quoted_expression);

			return;
		}
		// (vm-cond <waiting> ([(t1 c1) ... (tN cN)] environment))
		case OP_VM_COND: {
			LispNodeRC &waiting = vm_op_state;

			LispNodeRC &evaluation_pairs = vm_op_arguments->head->item;
			const LispNodeRC &environment = vm_op_arguments->head->next->item;

			if(waiting == atom_false && evaluation_pairs == list_empty) {
				data_push(list_empty);
				vm_pop();

				return;
			}

			const LispNodeRC &current_pair = evaluation_pairs->head->item;
			const LispNodeRC &current_test = current_pair->head->item;

			if(waiting == atom_false) {
				vm_push(make3(make_operator(OP_VM_EVAL), list_empty, make2(current_test, environment)));
				waiting = atom_true;
			}
			else {
				LispNodeRC result = data_peek();
				data_pop();

				if(result == atom_true) {
					vm_pop();

					if(current_pair->get_head_pointer()->get_next_pointer()->get_next_pointer() == nullptr) {
						const LispNodeRC &current_consequent = current_pair->head->next->item;

						vm_push(make3(make_operator(OP_VM_EVAL), list_empty, make2(current_consequent, environment)));
					}
					else {
						// The consequent is a sequence of operations
						LispNodeRC current_consequent = LispNode::make_list(current_pair->get_head_pointer()->get_next_pointer());

						vm_push(make3(make_operator(OP_VM_BEGIN), make3(list_empty, list_empty, atom_false), make2(current_consequent, environment)));
					}

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
					vm_pop();

					if(type->number_i == OP_AND) {
						data_push(atom_true);
					}

					if(type->number_i == OP_OR) {
						data_push(atom_false);
					}

					return;
				}

				vm_push(make3(make_operator(OP_VM_EVAL), list_empty, make2(evaluation_items->head->item, environment)));
				waiting = atom_true;
			}
			else {
				LispNodeRC result = data_peek();
				data_pop();

				if(type->number_i == OP_AND && result == atom_false) {
					vm_pop();
					data_push(atom_false);

					return;
				}
				if(type->number_i == OP_OR && result == atom_true) {
					vm_pop();
					data_push(atom_true);

					return;
				}

				evaluation_items = make_cdr(evaluation_items);
				waiting = atom_false;
			}

			return;
		}
		// (vm-define (<OP_DEFINE/OP_SET_E> <waiting>) (input environment))
		case OP_VM_DEFINE: {
			const LispNodeRC &type = vm_op_state->head->item;
			LispNodeRC &waiting = vm_op_state->head->next->item;

			const LispNodeRC &input = vm_op_arguments->head->item;
			const LispNodeRC &environment = vm_op_arguments->head->next->item;

			const LispNodeRC &operation = input->head->item;
			const LispNodeRC &argument1 = input->head->next->item;
			const LispNodeRC &argument2 = input->head->next->next->item;

			bool is_define_lambda = argument1->is_list();

			LispNodeRC symbol = is_define_lambda ? make_car(argument1) : input->head->next->item;

			if(waiting == atom_false) {
				if(count_members(input) < 3) {
					print_error(input->head->item->data, "missing arguments\n");
					vm_finish();

					return;
				}

				if(!symbol->is_atom() || !symbol->is_pure()) {
					print_error(input->head->item->data, "argument type error\n");
					vm_finish();

					return;
				}

				// Extend the frame on environments
				if(type->number_i == OP_DEFINE) {
					*context_environment = make_cons(make2(symbol, list_empty), environment);;
				}

				LispNodeRC expression = argument2;

				if(is_define_lambda) {
					LispNodeRC lambda_parameters = make_cdr(argument1);
					LispNodeRC lambda_expression = LispNode::make_list(input->get_head_pointer()->get_next_pointer()->get_next_pointer());

					expression = make_cons(make_operator(OP_LAMBDA), make_cons(lambda_parameters, lambda_expression));
				}

				vm_push(make3(make_operator(OP_VM_EVAL), list_empty, make2(expression, *context_environment)));
				waiting = atom_true;
			}
			else {
				LispNodeRC evaluated_expression = data_peek();
				data_pop();

				make_query_optional_replace(symbol, *context_environment, evaluated_expression);

				vm_pop();
				data_push(list_empty);
			}

			return;
		}
		// (vm-begin (<saved_context_environment> <current_closure> <waiting>) (evaluation_items environment))
		case OP_VM_BEGIN: {
			LispNodeRC &saved_context_environment = vm_op_state->head->item;
			const LispNodeRC &current_closure = vm_op_state->head->next->item;
			LispNodeRC &waiting = vm_op_state->head->next->next->item;

			if(waiting == atom_false) {
				// Get a non-const reference to the new environment
				LispNodeRC &new_environment = vm_op_arguments->head->next->item;

				saved_context_environment = LispNode::make_data(LispType::AtomData, context_environment);
				context_environment = &new_environment;

				vm_push(make3(make_operator(OP_VM_EVAL_LIST), make2(atom_true, atom_false), vm_op_arguments));
				waiting = atom_true;
			}
			else {
				context_environment = reinterpret_cast<LispNodeRC *>(saved_context_environment->data);
				saved_context_environment->data = nullptr;

				vm_pop();
			}

			return;
		}
		// (vm-apply (<arity> <closure_mode> <waiting>) (input environment))
		case OP_VM_APPLY: {
			const LispNodeRC &arity = vm_op_state->head->item;
			const LispNodeRC &closure_mode = vm_op_state->head->next->item;
			LispNodeRC &waiting = vm_op_state->head->next->next->item;

			if(waiting == atom_false && closure_mode == atom_true) {
				vm_push(make3(make_operator(OP_VM_EVAL_LIST), make2(atom_false, atom_false), make2(make_cdr(input), environment)));
				waiting = atom_true;
			}
			else {
				LispNodeRC evaluated_input = list_empty;

				if(closure_mode == atom_true) {
					for(size_t i = 0; i < arity->number_i; i++) {
						evaluated_input = make_cons(data_peek(), evaluated_input);
						data_pop();
					}

					// Add the original operator to the front of the evaluated arguments
					evaluated_input = make_cons(input->head->item, evaluated_input);
				}

				bool tail_situation = false;
				unsigned int tail_begin_blocks_found = 0;

				// Check for tail-recursion
				if(evaluation_stack->head->next != nullptr) {
					for(Box *current_box = evaluation_stack->get_head_pointer()->get_next_pointer(); current_box != nullptr; current_box = current_box->get_next_pointer()) {
						const LispNodeRC &vm_next = current_box->item;
						const LispNodeRC &vm_next_op = vm_next->head->item;

						if(vm_next_op->number_i == OP_VM_BEGIN) {
							tail_begin_blocks_found++;

							const LispNodeRC &vm_next_op_state = vm_next->head->next->item;
							const LispNodeRC &current_closure = vm_next_op_state->head->next->item;

							if(current_closure == input->head->item) {
								tail_situation = true;
								break;
							}
						}
						else {
							break;
						}
					}
				}

				LispNodeRC lambda_application = make_lambda_application(closure_mode == atom_true ? evaluated_input : input, environment);

				if(lambda_application == nullptr) {
					// Error message printed in the make_lambda_application() function
					vm_finish();
					return;
				}

				const LispNodeRC &new_expression = lambda_application->head->item;
				const LispNodeRC &new_environment = lambda_application->head->next->item;

				vm_pop();

				if(tail_situation) {
					// Complete the previous begins, then make a new one on the same stack level

					for(auto i = 0; i < tail_begin_blocks_found; i++) {
						const LispNodeRC &completed_begin = vm_peek();
						const LispNodeRC &completed_begin_vm_state = completed_begin->head->next->item;
						const LispNodeRC &saved_context_environment = completed_begin_vm_state->head->item;

						context_environment = reinterpret_cast<LispNodeRC *>(saved_context_environment->data);
						saved_context_environment->data = nullptr;

						vm_pop();
					}
				}

				vm_push(make3(make_operator(OP_VM_BEGIN), make3(list_empty, closure_mode == atom_true ? input->head->item : list_empty, atom_false), make2(new_expression, new_environment)));
			}

			return;
		}
		// (vm-eval '() (input environment))
		case OP_VM_EVAL: {
			vm_pop();

			if(!eval_reduce(input, environment)) {
				vm_finish();
			}

			return;
		}
		// (vm-load (<type>, <waiting>) (input environment))
		case OP_VM_LOAD: {
#ifdef INITIAL_ENVIRONMENT
			const LispNodeRC &type = vm_op_state->head->item;
			LispNodeRC &waiting = vm_op_state->head->next->item;

			if(waiting == atom_false) {
				if(count_members(input) != 2) {
					print_error(input->head->item->data, "missing or extra arguments\n");
					vm_finish();

					return;
				}

				const LispNodeRC &symbol = input->head->next->item;

				vm_push(make3(make_operator(OP_VM_EVAL), list_empty, make2(symbol, environment)));

				waiting = atom_true;
			}
			else {
				LispNodeRC evaluated_symbol = data_peek();
				data_pop();

				if(type->number_i == OP_LOAD) {
					int index;
					char *value = nullptr;

					if((index = get_lambda_index(evaluated_symbol->data)) != -1) {
						value = lambda_strings[index];
					}

					if((index = get_macro_index(evaluated_symbol->data)) != -1) {
						value = macro_strings[index];
					}

					LispNodeRC load_expression = make3(make_operator(OP_DEFINE), evaluated_symbol, parse_expression(value, false));

					vm_pop();
					vm_push(make3(make_operator(OP_VM_EVAL), list_empty, make2(load_expression, environment)));
				}
				else {
					LispNodeRC unload_expression = make3(make_operator(OP_SET_E), evaluated_symbol, atom_false);

					vm_pop();
					vm_push(make3(make_operator(OP_VM_EVAL), list_empty, make2(input, environment)));
				}
			}
#else
			print_error("vm-load", "no compiled support\n");
			vm_finish();
#endif /* INITIAL_ENVIRONMENT */

			return;
		}
		// (vm-call <arity> (input environment))
		case OP_VM_CALL: {
			LispNodeRC &arity = vm_op_state;

			LispNodeRC evaluated_input = list_empty;

			bool is_apply = input->is_operation(OP_APPLY);

			if(is_apply) {
				evaluated_input = data_peek();
				data_pop();

				if(!evaluated_input->is_list()) {
					print_error("vm-call", "last argument must be list\n");
					vm_finish();

					return;
				}
			}

			// If it is an apply operation, we already collected one evaluated input
			int to_collect = (is_apply ? arity->number_i - 1 : arity->number_i);

			for(size_t i = 0; i < to_collect; i++) {
				evaluated_input = make_cons(data_peek(), evaluated_input);
				data_pop();
			}

			// If it is an apply operation, the first input is the operation and it has
			// already been added
			if(is_apply) {
				vm_pop();
				vm_push(make3(make_operator(OP_VM_EVAL), list_empty, make2(evaluated_input, environment)));

				return;
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
				print_error(input->head->item->data, "evaluation error\n");
				vm_finish();

				return;
			}

			vm_pop();
			data_push(result);

			return;
		}
		// (vm-eval-list (<discard-itermediary> <waiting>) (evaluation_items environment))
		case OP_VM_EVAL_LIST: {
			const LispNodeRC &discard_intermediary = vm_op_state->head->item;
			LispNodeRC &waiting = vm_op_state->head->next->item;

			LispNodeRC &evaluation_items = vm_op_arguments->head->item;
			const LispNodeRC &environment = vm_op_arguments->head->next->item;

			if(evaluation_items == list_empty) {
				data_push(list_empty);
				vm_pop();

				return;
			}

			// If we are working in a begin operator
			if(waiting == atom_true && discard_intermediary == atom_true) {
				data_pop();
			}

			bool last_item = (evaluation_items->head->next == nullptr);

			// For tail-recursion
			if(last_item) {
				vm_pop();
			}

			vm_push(make3(make_operator(OP_VM_EVAL), list_empty, make2(make_car(evaluation_items), environment)));

			if(last_item) {
				return;
			}

			evaluation_items = make_cdr(evaluation_items);
			waiting = atom_true;

			return;
		}
		default:
			top->print();
			print_error(" at vm_step()", "unknown operation\n");
			vm_finish();

			return;
	}
}

LispNodeRC eval_expression(const LispNodeRC input, const LispNodeRC environment) {
	vm_reset();
	vm_push(make3(make_operator(OP_VM_EVAL), list_empty, make2(input, environment)));

	while(evaluation_stack != list_empty) {
		vm_step();
	}

	if(data_stack == list_empty) {
		return nullptr;
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
			Deallocate(input_string);
			break;
		}

		if(strcmp(input_string, "\n") == 0) {
			Deallocate(input_string);
			continue;
		}

		LispNodeRC input;

		if((input = parse_expression(input_string)) == nullptr) {
			fputs("Error reading expression\n", stdout);
			continue;
		}

		LispNodeRC output;

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
