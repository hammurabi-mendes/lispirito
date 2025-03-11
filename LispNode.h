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

	bool is_atom();
	bool is_list();

	bool is_pure();
	bool is_boolean();
	bool is_string();
	bool is_character();
	bool is_numeric();
	bool is_numeric_integral();
	bool is_numeric_real();

	void promoteReal();
	void demoteReal();

	void print();
};

struct Box {
	LispNodeRC item;
	BoxRC next;

	Box(const LispNodeRC &item): item{item}, next{nullptr} {}
	Box(LispNodeRC &&item) noexcept: item{item}, next{nullptr} {}

	Box *get_next_pointer() const {
		return next.get_pointer();
	}
};

#endif /* LISP_NODE_H */