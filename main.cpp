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
constexpr unsigned int MAX_TOKEN_SIZE = 64;

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

// Global file descriptor where the next input should come from or where the next output should go to
FILE *global_descriptor_input;
FILE *global_descriptor_output;

// Forward declarations
char *read_expression(FILE *descriptor);
LispNodeRC parse_expression(const char *buffer);
LispNodeRC eval_expression(LispNodeRC input, LispNodeRC environment);

void print_error(const LispNodeRC &input, const char *message) {
	input->print();
	fputs(": ", stdout);
	fputs(message, stdout);
}

void print_error(const char *context, const char *message) {
	fputs(context, stdout);
	fputs(": ", stdout);
	fputs(message, stdout);
}

bool empty_lisp_expression(const char *message) {
	for(const char *current = message; *current != '\0'; current++) {
		if(!isspace(*current)) {
			return false;
		}

		// Ignore commented sections
		if(*current == ';') {
			break;
		}
	}

	return true;
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

LispNodeRC make4(const LispNodeRC &first, const LispNodeRC &second, const LispNodeRC &third, const LispNodeRC &fourth) {
	LispNode *result = new LispNode(LispType::List);

	result->head = new Box(first);
	result->head->next = new Box(second);
	result->head->next->next = new Box(third);
	result->head->next->next->next = new Box(fourth);

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

	Box *second_element_box = list->get_pointer(1);

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
		Box *substituted_box = new Box(make_substitution(old_symbol, new_symbol, current_expression_box->item)); 

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

char *read_expression(FILE *descriptor = stdin) {
	// Allocated here once
	static char *read_buffer = static_cast<char *>(Allocate(MAX_EXPRESSION_SIZE));

	// This should never happen
	if(!read_buffer) {
        fputs("Out of memory: halt", stdout);
		return nullptr;
	}

#ifdef TARGET_C64
	if(descriptor == stdin) {
		join_lines = true;
		join_character = '\n';

		int result = terminal_getline(read_buffer, MAX_EXPRESSION_SIZE, false);

		for(int i = 0; i < result; i++) {
			if(read_buffer[i] == ';') {
				read_buffer[i] = '\n';
				read_buffer[i + 1] = '\0';

				break;
			}
		}

		return read_buffer;
	}
#endif // TARGET_C64

	int nread = 0;
	int total_open = 0;
	int total_close = 0;

	while(true) {
		char *last_line;

		if((last_line = fgets(read_buffer + nread, MAX_EXPRESSION_SIZE - nread, descriptor)) == nullptr) {
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

			if(*current == ';') {
				// This is safe because last_line is assumed to be null-terminated
				// (so there's an extra character after current)
				current[0] = '\n';
				current[1] = '\0';

				break;
			}
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

char *get_next_token(const char *buffer, size_t buffer_length, size_t &position) {
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

	// Else consider the following:
	// - Case 1: quoted strings
	// - Case 2: everything else

	int result_position = 0;

	if(current == '"') {
		// Case 1: quoted strings

		do {
			// Collect any character before the end quote
			result[result_position] = current;
			result_position++;

			// Go to next character
			position++;
			current = buffer[position];
		} while(current != '\0' && current != '"');

		if(current == '\0') {
			return nullptr;
		}

		result[result_position] = current;
		result_position++;

		position++;
	}
	else {
		// Case 2: everything else

		do {
			// Collect the non-space, non-parenthesis, non-quote character
			result[result_position] = current;
			result_position++;

			// Go to next character
			position++;
			current = buffer[position];
		} while(current != '\0' && current != '(' && current != ')' && current != '\'' && !isspace(current));
	}

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

	for(size_t i = 0; i < token_length; i++) {
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

LispNode *parse_atom(char *token) {
	int output = examine_string(token);

	// Try first true and false literals

	if(strcmp(token, "#t") == 0) {
		return atom_true.get_pointer();	
	}

	if(strcmp(token, "#f") == 0) {
		return atom_false.get_pointer();	
	}

	LispNode *result = new LispNode(LispType::AtomPure);

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

LispNode *parse_expression(const char *buffer, size_t buffer_length, size_t &position, bool &error) {
	char *token = get_next_token(buffer, buffer_length, position);

	if(token == nullptr) {
		error = true;

		return nullptr;
	}

	if(strcmp(token, "'") == 0) {
		LispNode *quoted = parse_expression(buffer, buffer_length, position, error);

		if(error) {
			return nullptr;
		}

		LispNode *result = new LispNode(LispType::List);

		result->head= new Box(make_operator(OP_QUOTE));
		result->head->next = new Box(quoted);

		return result;
	}
	
	if(strcmp(token, "(") == 0) {
		LispNode *result = new LispNode(LispType::List);
		result->head = nullptr;

		Box *last_box = nullptr;
		LispNode *member;

		while((member = parse_expression(buffer, buffer_length, position, error)) != nullptr) {
			if(error) {
				break;
			}

			Box *member_box = new Box(member);

			if(last_box == nullptr) {
				result->head = member_box;
			}
			else {
				last_box->next = member_box;
			}

			last_box = member_box;
		}

		if(error || result->head == nullptr) {
			delete result;

			if(error) {
				return nullptr;
			}
			else {
				return list_empty.get_pointer();
			}
		}

		return result;
	}

	if(strcmp(token, ")") == 0) {
		return nullptr;
	}

	return parse_atom(token);
}

LispNodeRC parse_expression(const char *buffer) {
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

unsigned int count_members(const LispNodeRC &list) {
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
    	case OP_NEWLINE: {
			fputs("\n", stdout);

			return list_empty;
		}
		case OP_CURRENT_ENVIRONMENT:
			return environment;
	}

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
		case OP_NULL_Q:
			return (output1 == list_empty) ? atom_true : atom_false;
		case OP_PAIR_Q:
			return (output1->is_list() && output1 != list_empty) ? atom_true : atom_false;
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
			result->number_i = output1->number_r.as_i();
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

			result = parse_atom(output1->data);

			if(result == nullptr || !result->is_numeric()) {
				return nullptr;
			}

			break;
		case OP_STRING_DATA: {
			if(!output1->is_string()) {
				return nullptr;
			}

			// Note: we do not duplicate data, only convert it in place
			output1->type = LispType::AtomDataOwned;
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
			output1->print();

			return list_empty;
		case OP_READ: {
			char *input_string;
			
			do {
				input_string = read_expression(output1->is_data() ? ((FILE *) output1->data) : stdin);

				if(input_string == nullptr) {
					return list_empty;
				}
			} while(empty_lisp_expression(input_string));

			return parse_expression(input_string);
		}
#ifdef IO_AVAILABLE
		case OP_CLOSE:
			if(!output1->is_data()) {
				return nullptr;
			}

			fclose((FILE *) output1->data);

			return list_empty;
		case OP_LOAD_E:
			global_descriptor_input = (output1->is_data() ? ((FILE *) output1->data) : stdin);

			return list_empty;
		case OP_SAVE_E:
			global_descriptor_output = (output1->is_data() ? ((FILE *) output1->data) : stdout);

			return list_empty;
#endif // IO_AVAILABLE
		case OP_MEM_ALLOC:
			result = new LispNode(LispType::AtomDataOwned);
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
	}

	return result;
}

LispNodeRC eval_gen2(const LispNodeRC &input, const LispNodeRC &environment) {
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
#ifdef IO_AVAILABLE
		case OP_OPEN: {
			if(!output1->is_string() || !output2->is_string()) {
				return nullptr;
			}

			FILE *descriptor = fopen(output1->data, output2->data);

			if (!descriptor) {
				return nullptr;
			}

			return LispNode::make_data(LispType::AtomDataExternal, static_cast<void *>(descriptor));
		}
#endif //IO_AVAILABLE
    	case OP_WRITE: {
			output1->print(output2->is_data() ? ((FILE *) output2->data) : stdout);

			return list_empty;
		}
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
	}

	return result;
}

const LispNodeRC &eval_procedure(const LispNodeRC &input, const LispNodeRC &environment) {
	if(count_members(input) < 3) {
		print_error("lambda", "argument count error\n");

		return list_empty;
	}

	const LispNodeRC &argument1 = input->head->next->item;

	if(!argument1->is_list()) {
		print_error("lambda", "argument type error\n");

		return list_empty;
	}

	for(Box *current_parameter_box = argument1->get_head_pointer(); current_parameter_box != nullptr; current_parameter_box = current_parameter_box->get_next_pointer()) {
		if(!current_parameter_box->item->is_atom() || !current_parameter_box->item->is_pure()) {
			print_error("lambda", "argument type error\n");

			return list_empty;
		}
	}

	return input;
}

const LispNodeRC &eval_closure(const LispNodeRC &input, const LispNodeRC &environment) {
	return input;
}

const LispNodeRC &eval_macro(const LispNodeRC &input, const LispNodeRC &environment) {
	return eval_procedure(input, environment);
}

LispNodeRC eval_lambda(const LispNodeRC &input, const LispNodeRC &environment) {
	if(eval_procedure(input, environment) == list_empty) {
		return nullptr;
	}

	return make4(make_operator(OP_CLOSURE), list_empty, input, environment);
}

#ifndef SEPARATE_FRAMES
inline const LispNodeRC &make_environment(const LispNodeRC &environment) {
	return environment;
}
#else
LispNodeRC make_environment(const LispNodeRC &environment) {
	// Creates an environment with a dummy entry to separate frames
	LispNodeRC new_environment = make_cons(make2(atom_false, atom_false), environment);

	return new_environment;
}
#endif /* SEPARATE_FRAMES */

LispNodeRC make_lambda_macro_application(const LispNodeRC &input, const LispNodeRC &environment) {
	const LispNodeRC &closure_or_macro = input->head->item;

	int operator_index = closure_or_macro->head->item->number_i;

	// Defines if we operate on macro substitution mode or in closure application mode
	bool is_closure = (operator_index == OP_CLOSURE);
	bool is_macro = (operator_index == OP_MACRO);

	const LispNodeRC &procedure = (is_closure ? closure_or_macro->head->next->next->item : closure_or_macro);
	const LispNodeRC &procedure_parameters = procedure->head->next->item;

	// Evaluate the function by setting a new environment for the defined parameters

	// Used when is_macro == #t:
	//     Macro expansion: substitutes non-evaluated parameters into arguments in the original expression
	LispNodeRC new_expression = LispNode::make_list(procedure->get_pointer(2));
	// Used when is_macro == #f:
	//     Eager evaluation: creates a new environment binding parameters to their eagerly-evaluated arguments
	LispNodeRC new_environment = (is_closure ? make_environment(closure_or_macro->head->next->next->next->item) : environment);

	if(is_closure) {
		const LispNodeRC &closure_name = closure_or_macro->head->next->item;

		if(closure_name != list_empty && make_query_optional_replace(closure_name, new_environment) == nullptr) {
			new_environment = make_cons(make2(closure_name, closure_or_macro), new_environment);
		}
	}

	bool packed_dot = false;

	Box *current_parameter_box = procedure_parameters->get_head_pointer();
	Box *current_argument_box = input->get_pointer(1);

	while(current_parameter_box != nullptr || current_argument_box != nullptr) {
		if(current_parameter_box == nullptr || current_argument_box == nullptr) {
			print_error("operator application", "argument count error\n");

			return nullptr;
		}

		// Evaluate the argument using the old environment
		LispNodeRC parameter = current_parameter_box->item;
		LispNodeRC argument = current_argument_box->item;

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

// VM data structures, variables and functions

struct VMStackFrame {
	int8_t op;
	bool waiting;

	uint8_t type;
	uint8_t arity;

	LispNodeRC input;
	LispNodeRC environment;

	LispNodeRC *saved_context_environment; // For OP_VM_BEGIN and OP_VM_LET
	LispNodeRC extra;                      // For closure identification on tail recursion

#ifdef TARGET_6502
	uint8_t _pad[4];                       // Pad to 16B to ease the 6502
#endif // TARGET_6502

	VMStackFrame(): op{0}, waiting{false},
	                type{0}, arity{0},
	                input{nullptr}, environment{nullptr},
	                saved_context_environment{nullptr}, extra{nullptr} {}
};

#ifdef TARGET_6502
	using StackSizeType = uint8_t;
#else
	using StackSizeType = unsigned int;
#endif

constexpr StackSizeType EVALUATION_STACK_SIZE = 128;
constexpr StackSizeType DATA_STACK_SIZE = 128;

VMStackFrame *evaluation_stack;
LispNodeRC *data_stack;

StackSizeType vm_top;
StackSizeType data_top;

StackSizeType vm_maximum;
StackSizeType data_maximum;

VMStackFrame *vm_push_operation(int8_t op, const LispNodeRC &input, const LispNodeRC &environment) {
	VMStackFrame *frame = &evaluation_stack[vm_top];

	frame->op = op;
	frame->waiting = false;

	frame->input = input;
	frame->environment = environment;

	frame->type = 0;
	frame->arity = 0;
	frame->extra = list_empty;
	frame->saved_context_environment = nullptr;

	vm_top++;

	if(vm_top > vm_maximum) {
		vm_maximum = vm_top;
	}

	return frame;
}

inline void vm_push_type(int8_t op, const LispNodeRC &input, const LispNodeRC &environment, uint8_t type) {
	VMStackFrame *frame = vm_push_operation(op, input, environment);

	frame->type = type;
}

inline void vm_push_arity(int8_t op, const LispNodeRC &input, const LispNodeRC &environment, uint8_t arity) {
	VMStackFrame *frame = vm_push_operation(op, input, environment);

	frame->arity = arity;
}

inline void vm_push_type_arity(int8_t op, const LispNodeRC &input, const LispNodeRC &environment, uint8_t type, uint8_t arity) {
	VMStackFrame *frame = vm_push_operation(OP_VM_APPLY, input, environment);

	frame->type = type;
	frame->arity = arity;
}

inline VMStackFrame &vm_peek() {
	return evaluation_stack[vm_top - 1];
}

inline void vm_pop() {
	vm_top--;
}

inline void data_push(const LispNodeRC &node) {
	data_stack[data_top] = node;
	data_top++;

	if(data_top > data_maximum) {
		data_maximum = data_top;
	}
}

inline LispNodeRC &data_peek() {
	return data_stack[data_top - 1];
}

inline void data_pop() {
	data_top--;
}

void vm_finish() {
	vm_top = 0;
	data_top = 0;

	context_environment = &global_environment;
}

void vm_reset() {
	vm_finish();

	vm_maximum = 0;
	data_maximum = 0;
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

			print_error(input, "evaluation error\n");
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
		ReduceMode operation_reduce_mode = operation_index >= 0 ? operator_reduce_modes[first->number_i] : Unspecified;

		switch(operation_reduce_mode) {
			case Normal0:
			case Normal1:
			case Normal2:
			case Normal3:
				// Normal:
				vm_push_arity(OP_VM_NORMAL, input, environment, (uint8_t) (operation_reduce_mode - Normal0));
				return true;

			case SpecialQuote:
				// Special: does not evaluate
				vm_push_operation(OP_VM_QUOTE, input, environment);
				return true;

			case SpecialCond:
				// Special:
				vm_push_operation(OP_VM_COND, make_cdr(input), environment);
				return true;

			case SpecialLogic:
				// Special:
				vm_push_type(OP_VM_LOGIC, make_cdr(input), environment, (uint8_t) operation_index);
				return true;

			case SpecialDefine:
				// Special:
				vm_push_type(OP_VM_DEFINE, input, environment, (uint8_t) operation_index);
				return true;

			case SpecialBegin:
				// Special:
				vm_push_type(OP_VM_BEGIN, make_cdr(input), make_environment(environment), (uint8_t) operation_index);
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
				vm_push_arity(OP_VM_NORMAL, input, environment, (uint8_t) (count_members(input) - 1));
				return true;
			
			default:
				print_integral(operation_reduce_mode);
				print_error(" at eval_reduce()", "unknown reduce type\n");
				vm_finish();
		}
	}

	// Here, the operator is itself a list
	// 		1) If it has been evaluated, it should be a closure or a macro
	//		2) If it has not been evaluated, we create a new ("vm-first" ...)

	bool is_closure = first->is_operation(OP_CLOSURE);
	bool is_macro = first->is_operation(OP_MACRO);

	if(is_closure || is_macro) {
		vm_push_type_arity(OP_VM_APPLY, input, environment, (uint8_t) is_closure, (uint8_t) (count_members(input) - 1));
		return true;
	}

	if((first->is_atom() && first->is_pure()) || first->is_list()) {
		vm_push_operation(OP_VM_FIRST, input, environment);
		return true;
	}

	print_error(input, "evaluation error\n");

	return false;
}

void vm_step() {
	VMStackFrame &top = vm_peek();

	LispNodeRC input = top.input;
	LispNodeRC environment = top.environment;

	int operation_index = top.op;

	switch(operation_index) {
		// (vm-first <waiting> (input environment))
		case OP_VM_FIRST: {
			bool &waiting = top.waiting;

			if(waiting == false) {
				vm_push_operation(OP_VM_REDUCE, input->head->item, environment);
				waiting = true;
			}
			else {
				const LispNodeRC &result = data_peek();

				vm_pop();
				vm_push_operation(OP_VM_REDUCE, make_cons(result, make_cdr(input)), environment);

				data_pop();
			}

			return;
		}
		// (vm-normal <arity> (input environment))
		case OP_VM_NORMAL: {
			uint8_t arity = top.arity;

			if(count_members(input) != arity + 1) {
				print_error(input, "argument count error\n");
				vm_finish();

				return;
			}

			vm_pop();

			vm_push_arity(OP_VM_CALL, input, environment, arity);
			vm_push_type(OP_VM_REDUCE_LIST, make_cdr(input), environment, (uint8_t) false);

			return;
		}
		// (vm-quote (input environment))
		case OP_VM_QUOTE: {
			if(count_members(input) != 2) {
				print_error(input, "argument count error\n");
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
			bool &waiting = top.waiting;

			LispNodeRC &evaluation_pairs = top.input;

			if(waiting == false && evaluation_pairs == list_empty) {
				vm_pop();
				data_push(list_empty);

				return;
			}

			const LispNodeRC &current_pair = evaluation_pairs->head->item;
			const LispNodeRC &current_test = current_pair->head->item;

			if(waiting == false) {
				vm_push_operation(OP_VM_REDUCE, current_test, environment);
				waiting = true;
			}
			else {
				LispNodeRC result = data_peek();
				data_pop();

				if(result == atom_true) {
					if(current_pair->get_pointer(1) == nullptr) {
						// No consequent: just evaluate to the empty list

						vm_pop();
						data_push(list_empty);

						return;
					}

					if(current_pair->get_pointer(2) == nullptr) {
						LispNodeRC current_consequent = current_pair->head->next->item;

						vm_pop();
						vm_push_operation(OP_VM_REDUCE, current_consequent, environment);
					}
					else {
						// The consequent is a sequence of operations
						LispNodeRC current_consequent = LispNode::make_list(current_pair->get_pointer(1));

						vm_pop();
						vm_push_type(OP_VM_BEGIN, current_consequent, environment, (uint8_t) OP_BEGIN);
					}

					return;
				}

				evaluation_pairs = make_cdr(evaluation_pairs);
				waiting = false;
			}

			return;
		}
		// (vm-logic <OP_AND/OP_OR> <waiting> (evaluation_items environment))
		case OP_VM_LOGIC: {
			uint8_t type = top.type;
			bool &waiting = top.waiting;

			LispNodeRC &evaluation_items = top.input;

			if(evaluation_items == list_empty) {
				vm_pop();

				if(type == OP_AND) {
					data_push(atom_true);
				}

				if(type == OP_OR) {
					data_push(atom_false);
				}

				return;
			}

			if(waiting == false) {
				vm_push_operation(OP_VM_REDUCE, evaluation_items->head->item, environment);
				waiting = true;

				return;
			}

			LispNodeRC result = data_peek();
			data_pop();

			if(type == OP_AND && result == atom_false) {
				vm_pop();
				data_push(atom_false);

				return;
			}
			if(type == OP_OR && result == atom_true) {
				vm_pop();
				data_push(atom_true);

				return;
			}

			bool last_item = (evaluation_items->head->next == nullptr);

			// For tail-recursion
			if(last_item) {
				vm_pop();
			}

			vm_push_operation(OP_VM_REDUCE, evaluation_items->head->item, environment);

			if(last_item) {
				return;
			}

			evaluation_items = make_cdr(evaluation_items);
			waiting = false;

			return;
		}
		// (vm-define <OP_DEFINE/OP_SET_E> <waiting> (input environment))
		case OP_VM_DEFINE: {
			uint8_t type = top.type;
			bool &waiting = top.waiting;

			const LispNodeRC &operation = input->head->item;
			const LispNodeRC &argument1 = input->head->next->item;
			const LispNodeRC &argument2 = input->head->next->next->item;

			bool is_define_lambda = argument1->is_list();

			const LispNodeRC &symbol = is_define_lambda ? argument1->head->item : input->head->next->item;

			if(waiting == false) {
				if(count_members(input) < 3) {
					print_error(input, "argument count error\n");
					vm_finish();

					return;
				}

				if(!symbol->is_atom() || !symbol->is_pure()) {
					print_error(symbol, "argument type error\n");
					vm_finish();

					return;
				}

				LispNodeRC expression = argument2;

				if(is_define_lambda) {
					LispNodeRC lambda_parameters = make_cdr(argument1);
					LispNodeRC lambda_expression = LispNode::make_list(input->get_pointer(2));

					expression = make_cons(make_operator(OP_LAMBDA), make_cons(lambda_parameters, lambda_expression));
				}

				// Evaluate using the provided environment
				vm_push_operation(OP_VM_REDUCE, expression, environment);

				waiting = true;
			}
			else {
				const LispNodeRC &evaluated_expression = data_peek();

				if(evaluated_expression->is_operation(OP_CLOSURE)) {
					evaluated_expression->head->next->item = symbol;
				}

				if(type == OP_DEFINE) {
					// Extend the current environment
					*context_environment = make_cons(make2(symbol, list_empty), *context_environment);;
				}

				make_query_optional_replace(symbol, *context_environment, evaluated_expression);

				data_pop();

				vm_pop();
				data_push(list_empty);
			}

			return;
		}
		// (vm-begin <OP_LET/OP_LET_STAR/OP_BEGIN <waiting> <saved_context_environment> (<begin/let (definition_list)> statements environment))
		case OP_VM_BEGIN: {
			uint8_t type = top.type;
			bool &waiting = top.waiting;
			LispNodeRC *&saved_context_environment = top.saved_context_environment;

			if(waiting == false) {
				if(input == list_empty) {
					vm_pop();
					data_push(list_empty);

					return;
				}

				saved_context_environment = context_environment;

				if(type == OP_LET || type == OP_LET_STAR) {
					// The first element of input is a definitions_list
					vm_push_type(OP_VM_REDUCE_LIST, make_cdr(input), environment, (uint8_t) true);
				}
				else {
					vm_push_type(OP_VM_REDUCE_LIST, input, environment, (uint8_t) true);
				}
				context_environment = &(vm_peek().environment);

				if(type == OP_LET || type == OP_LET_STAR) {
					LispNodeRC &definitions = input->head->item;

					vm_push_type(OP_VM_DEFINE_LIST, definitions, *context_environment, type);
				}

				waiting = true;
			}
			else {
				context_environment = saved_context_environment;

				vm_pop();
			}

			return;
		}
		// (vm-apply <arity> <type: 1 = closure_mode 0 = macro_mode> <waiting> (input environment))
		case OP_VM_APPLY: {
			uint8_t arity = top.arity;
			bool closure_mode = (bool) top.type;
			bool &waiting = top.waiting;

			if(waiting == false && closure_mode == true) {
				vm_push_type(OP_VM_REDUCE_LIST, make_cdr(input), environment, (uint8_t) false);
				waiting = true;
			}
			else {
				LispNodeRC evaluated_input = list_empty;

				if(closure_mode == true) {
					for(uint8_t i = 0; i < arity; i++) {
						evaluated_input = make_cons(data_peek(), evaluated_input);
						data_pop();
					}

					// Add the original operator to the front of the evaluated arguments
					evaluated_input = make_cons(input->head->item, evaluated_input);
				}

				bool tail_situation = false;
				unsigned int tail_begin_blocks_found = 0;

				// Check for tail-recursion
				if(vm_top >= 2) {
					for(int i = vm_top - 2; i >= 0; i--) {
						const VMStackFrame &vm_next = evaluation_stack[i];

						if(vm_next.op == OP_VM_BEGIN) {
							tail_begin_blocks_found++;

							const LispNodeRC &current_closure = vm_next.extra;

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

				LispNodeRC lambda_application = make_lambda_macro_application(closure_mode ? evaluated_input : input, environment);

				if(lambda_application == nullptr) {
					// Error message printed in the make_lambda_macro_application() function
					vm_finish();
					return;
				}

				const LispNodeRC &new_expression = lambda_application->head->item;
				const LispNodeRC &new_environment = lambda_application->head->next->item;

				vm_pop();

				if(tail_situation) {
					// Complete the previous begins, then make a new one on the same stack level

					for(size_t i = 0; i < tail_begin_blocks_found; i++) {
						VMStackFrame &completed_begin = vm_peek();

						context_environment = completed_begin.saved_context_environment;

						vm_pop();
					}
				}

				vm_push_type(OP_VM_BEGIN, new_expression, new_environment, (uint8_t) OP_BEGIN);
				vm_peek().extra = closure_mode ? input->head->item : list_empty;
			}

			return;
		}
		// (vm-eval '() (input environment))
		case OP_VM_REDUCE: {
			vm_pop();

			if(!eval_reduce(input, environment)) {
				vm_finish();
			}

			return;
		}
		// (vm-call <arity> (input environment))
		case OP_VM_CALL: {
			uint8_t arity = top.arity;

			bool is_apply = input->is_operation(OP_APPLY);
			bool is_eval = input->is_operation(OP_EVAL);

			Box *evaluated_parameter_sequence = nullptr;

			if(is_apply) {
				const LispNodeRC &last_evaluated_argument = data_peek();

				if(!last_evaluated_argument->is_list()) {
					print_error(last_evaluated_argument, "argument type error\n");
					vm_finish();

					return;
				}

				evaluated_parameter_sequence = last_evaluated_argument->get_head_pointer();
				data_pop();
			}

			if(is_eval) {
				LispNodeRC eval_environment = data_peek();
				data_pop();

				LispNodeRC eval_expression = data_peek();
				data_pop();

				vm_pop();
				vm_push_operation(OP_VM_REDUCE, eval_expression, eval_environment);

				return;
			}

			// We already initialized the evaluated parameter sequence to the last argument of an apply operation
			uint8_t to_collect = (is_apply ? arity - 1 : arity);

			for(uint8_t i = 0; i < to_collect; i++) {
				evaluated_parameter_sequence = new Box(data_peek(), evaluated_parameter_sequence);
				data_pop();
			}

			// If it is an apply operation, the first input is the operation and it has
			// already been added
			if(is_apply) {
				LispNodeRC apply_operation = LispNode::make_list(evaluated_parameter_sequence);

				vm_pop();
				vm_push_operation(OP_VM_REDUCE, apply_operation, environment);

				return;
			}

			// Add the original operator to the front of the evaluated arguments
			Box *evaluated_input_sequence = new Box(input->head->item, evaluated_parameter_sequence);
			LispNodeRC generic_operation = LispNode::make_list(evaluated_input_sequence);

			LispNodeRC result;

			switch(arity) {
				case 0:
					result = eval_gen0(generic_operation, environment);
					break;
				case 1:
					result = eval_gen1(generic_operation, environment);
					break;
				case 2:
					result = eval_gen2(generic_operation, environment);
					break;
				case 3:
					result = eval_gen3(generic_operation, environment);
					break;
			}

			if(result == nullptr) {
				print_error(input, "evaluation error\n");
				vm_finish();

				return;
			}

			vm_pop();
			data_push(result);

			return;
		}
		// (vm-define-list (definition_items environment))
		case OP_VM_DEFINE_LIST: {
			uint8_t type = top.type;
			bool &waiting = top.waiting;
			LispNodeRC &definition_items = top.input;

			if(waiting == false) {
				if(!definition_items->is_list()) {
					print_error(definition_items, "argument type error\n");
					vm_finish();

					return;
				}
			}

			// If we performed a definition, cleanup the data stack
			if(waiting == true) {
				data_pop();
			}

			if(definition_items == list_empty) {
				// An empty definition list does not insert anything into the data stack
				vm_pop();

				return;
			}

			const LispNodeRC &current_definition = definition_items->head->item;

			if(type == OP_LET) {
				// let
				vm_push_type(OP_VM_DEFINE, make_cons(make_operator(OP_DEFINE), current_definition), environment, (uint8_t) OP_DEFINE);
			}
			else {
				// let*
				vm_push_type(OP_VM_DEFINE, make_cons(make_operator(OP_DEFINE), current_definition), *context_environment, (uint8_t) OP_DEFINE);
			}

			definition_items = make_cdr(definition_items);
			waiting = true;

			return;
		}
		// (vm-eval-list <discard-itermediary> <waiting> (evaluation_items environment))
		case OP_VM_REDUCE_LIST: {
			bool discard_intermediary = (bool) top.type;
			bool &waiting = top.waiting;

			LispNodeRC &evaluation_items = top.input;

			if(evaluation_items == list_empty) {
				// An empty evaluation list does not insert anything into the data stack
				vm_pop();

				return;
			}

			// If we are working in a begin operator
			if(waiting == true && discard_intermediary == true) {
				data_pop();
			}

			bool last_item = (evaluation_items->head->next == nullptr);

			// For tail-recursion
			if(last_item) {
				vm_pop();
			}

			vm_push_operation(OP_VM_REDUCE, evaluation_items->head->item, environment);

			if(last_item) {
				return;
			}

			evaluation_items = make_cdr(evaluation_items);
			waiting = true;

			return;
		}
		default:
			print_integral(top.op);
			print_error(" at vm_step()", "unknown operation\n");
			vm_finish();

			return;
	}
}

LispNodeRC eval_expression(const LispNodeRC input, const LispNodeRC environment) {
	vm_push_operation(OP_VM_REDUCE, input, environment);

	while(vm_top > 0) {
		if(vm_top >= EVALUATION_STACK_SIZE) {
			fputs("Stack overflow; use tail-recursion\n", stdout);

			return nullptr;
		}

		if(data_top >= DATA_STACK_SIZE) {
			fputs("Stack overflow; use tail-recursion\n", stdout);

			return nullptr;
		}

		vm_step();
	}

	if(data_top == 0) {
		return nullptr;
	}

	return data_peek();
}

void cleanup_stacks() {
	for(StackSizeType i = 0; i < vm_maximum; i++) {
		evaluation_stack[i].input = list_empty;
		evaluation_stack[i].environment = list_empty;
		evaluation_stack[i].extra = list_empty;
	}

	for(StackSizeType i = 0; i < data_maximum; i++) {
		data_stack[i] = list_empty;
	}
}

void cleanup() {
#ifdef OPTIONAL_MSGS
		fputs(";* cleaning up... ", stdout);
#endif /* OPTIONAL_MSGS */

	// Round 1: pre VM/data stack cleaning
	while(Allocator<LispNode>::process_deletions() == true || Allocator<Box>::process_deletions() == true) {
		// Keep cleaning...
	}

	cleanup_stacks();

	// Round 2: post VM/data stack cleaning
	while(Allocator<LispNode>::process_deletions() == true || Allocator<Box>::process_deletions() == true) {
		// Keep cleaning...
	}

#ifdef OPTIONAL_MSGS
		fputs("done\n", stdout);
#endif /* OPTIONAL_MSGS */
}

void initialize_stacks() {
	vm_maximum = EVALUATION_STACK_SIZE + 4;
	data_maximum = DATA_STACK_SIZE + 4;

	cleanup_stacks();
}

void loop_read_evaluate_print(FILE *descriptor_input, FILE *descriptor_output) {
	constexpr int MAXIMUM_INPUT_CONTEXTS = 8;

	static FILE *backup_descriptor_inputs[MAXIMUM_INPUT_CONTEXTS];
	int context_position = 0;

	bool interactive_input = (descriptor_input == stdin);

	LispNodeRC input;
	LispNodeRC output;

	while(true) {
		if(interactive_input) {
			cleanup_stacks();
			vm_reset();
		}

		// If a (save! fd) operation changed the global output descriptor,
		// the output will be written to that file until the current output descriptor is changed again
		if(descriptor_output != global_descriptor_output) {
			descriptor_output = global_descriptor_output;
		}

		// If a (load! fd) operation changed the global input descriptor,
		// obtain the input from that descriptor then return to the current input descriptor
		if(descriptor_input != global_descriptor_input) {
			if(context_position < MAXIMUM_INPUT_CONTEXTS) {
				backup_descriptor_inputs[context_position++] = descriptor_input;
				descriptor_input = global_descriptor_input;

				interactive_input = (descriptor_input == stdin);
				continue;
			}
		}

#ifdef OPTIONAL_MSGS
		fputs(";* free: ", stdout);
		print_integral(__heap_bytes_free());
		fputs("\n", stdout);
#endif /* OPTIONAL_MSGS */

		// Interactive input: print prompt
		if(interactive_input) {
			fputs("> ", stdout);
		}

		char *input_string = read_expression(descriptor_input);

		if(input_string == nullptr) {
			if(context_position > 0) {
				global_descriptor_input = backup_descriptor_inputs[--context_position];

				interactive_input = (descriptor_input == stdin);
				continue;
			}

			break;
		}

		// External input: print expression
		if(!interactive_input) {
			fputs(input_string, stdout);
		}

		if(empty_lisp_expression(input_string)) {
			continue;
		}

		if((input = parse_expression(input_string)) == nullptr) {
			fputs("Error reading expression\n", stdout);

			continue;
		}

		if((output = eval_expression(input, global_environment)) == nullptr) {
			fputs("Error evaluating expression\n", stdout);

			continue;
		}

		output->print(descriptor_output);
		fputs("\n", descriptor_output);
	}
}

void write_output(const LispNodeRC &expression, FILE *descriptor) {
	expression->print(descriptor);
}

int main(int argc, char **argv) {
#ifdef TARGET_6502
	__set_heap_limit(LISP_HEAP_SIZE);
#endif /* TARGET_6502 */

	// Initializes the allocator managers for LispNode and Box
	Allocator<LispNode>::init();
	Allocator<Box>::init();

	// Setup global constants

	atom_true = new LispNode(LispType::AtomBoolean);
	atom_true->data = strdup("#t");

	atom_false = new LispNode(LispType::AtomBoolean);
	atom_false->data = strdup("#f");

	list_empty = new LispNode(LispType::List);
	list_empty->head = nullptr;

	// Setup global environment

	global_environment = list_empty;

	// Setup global descriptors
	global_descriptor_input = stdin;
	global_descriptor_output = stdout;

	// Setup VM (4 entries extra so we check overflow only occasionally on vm_step())
	evaluation_stack = new VMStackFrame[EVALUATION_STACK_SIZE + 4];
	data_stack = new LispNodeRC[DATA_STACK_SIZE + 4];

	// Read-Eval-Print loop

	initialize_stacks();

	vm_reset();
	loop_read_evaluate_print(global_descriptor_input, global_descriptor_output);
	vm_finish();

	atom_true = nullptr;
	atom_false = nullptr;
	list_empty = nullptr;

	global_environment = nullptr;

	return EXIT_SUCCESS;
}
