#ifndef LISP_NODE_H
#define LISP_NODE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

// Forward declaration
struct LispNode;
struct Box;

#include "RCPointer.hpp"

using LispNodeRC = RCPointer<LispNode>;
using BoxRC = RCPointer<Box>;

enum LispType : unsigned char {
	AtomPure,
	AtomBoolean,
	AtomString,
	AtomCharacter,
	AtomNumericIntegral,
	AtomNumericReal,
	List
};

struct LispNode {
	LispType type;

	union {
		char *string;
		Integral number_i;
		Real number_r;
		BoxRC head;
	};

public:
	LispNode(LispType type);
	~LispNode();

	Box *get_head_pointer() const {
		return head.get_pointer();
	}

	bool operator==(const LispNode &other) const;

	bool is_atom() const;
	bool is_list() const;

	bool is_pure() const;
	bool is_boolean() const;
	bool is_string() const;
	bool is_character() const;
	bool is_numeric() const;
	bool is_numeric_integral() const;
	bool is_numeric_real() const;

	bool is_operator(char *op) const;

	void op_arithmetic(int operation, LispNodeRC &first, LispNodeRC &second);
	bool op_comparison(int operation, LispNodeRC &other);

	void op_arithmetic_integer(int operation, LispNodeRC &first, LispNodeRC &second);
	void op_arithmetic_real(int operation, LispNodeRC &first, LispNodeRC &second);
	bool op_comparison_integer(int operation, LispNodeRC &other);
	bool op_comparison_real(int operation, LispNodeRC &other);

	void promoteReal();
	void demoteReal();

	void print() const;
};

struct Box {
	LispNodeRC item;
	BoxRC next;

	Box(const LispNodeRC &item);

	Box *get_next_pointer() const {
		return next.get_pointer();
	}
};

#endif /* LISP_NODE_H */