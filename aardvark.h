#ifndef _AARDVARK_H
#define _AARDVARK_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

// Syntax tree node types
enum {
	SYNTAX_NONE = 0,
	TOKEN_IDENTIFIER,
	TOKEN_INTEGER,
	TOKEN_STRING,
	// Punctuation + Operators
	TOKEN_COMMA,
	TOKEN_L_PAREN,
	TOKEN_R_PAREN,
	TOKEN_PLUS,
	TOKEN_MINUS,
	TOKEN_MULTIPLY,
	TOKEN_DIVIDE,
	TOKEN_ASSIGN,	// =
	TOKEN_NOT,
	TOKEN_GREATER,
	TOKEN_LESS,
	TOKEN_EQUAL,	// ==
	TOKEN_NOT_EQUAL,
	TOKEN_GREATER_EQUAL,
	TOKEN_LESS_EQUAL,
	// Keywords
	TOKEN_DO,
	TOKEN_ELSE,
	TOKEN_END,
	TOKEN_FN,
	TOKEN_IF,
	TOKEN_RETURN,
	TOKEN_THEN,
	TOKEN_VAR,
	TOKEN_WHILE,
	// Syntax (grammar productions)
	SYNTAX_PROGRAM,
	SYNTAX_FUNCTION,
	SYNTAX_BLOCK,
	SYNTAX_DECLARATION,
	SYNTAX_ASSIGNMENT,
	SYNTAX_FUNCTION_CALL,
	SYNTAX_PARAMETER_LIST,
	SYNTAX_ARGUMENT_LIST,
	SYNTAX_RETURN,
	SYNTAX_IF,
	SYNTAX_WHILE,
	// Runtime
	RUNTIME_STANDARD_FUNCTION,
	RUNTIME_KNOWN_FUNCTION,
	RUNTIME_KNOWN_VARIABLE,
	RUNTIME_KNOWN_GLOBAL_VARIABLE,
};
typedef uint8_t	Syntax;

typedef union TokenData	TokenData;
union TokenData {
	uint64_t	identifier;
	int64_t		integerLiteral;
	const char*	stringLiteral;
};

typedef struct {
	TokenData	data;
	Syntax		syntax;
} Token;

typedef struct {
	Token*	tokens;
	size_t	tokenCapacity;
	size_t	tokenCount;
} TokenList;

typedef struct ParseNode	ParseNode;
struct ParseNode {
	union {
		TokenData	data;
		ParseNode*	function;
		ssize_t		stackIndex;
	};
	ParseNode*	children;
	uint16_t	childCapacity;
	uint16_t	childCount;
	Syntax		syntax;
};

typedef enum Type	Type;
enum Type {
	TYPE_NONE,
	TYPE_VOID,
	TYPE_INTEGER,
	TYPE_STRING,
};

typedef struct Data	Data;
struct Data {
	union {
		int64_t		integer;
		const char*	string;
	};
	Type	type;
};

TokenList tokenize(const char* chars, size_t count);
void printSyntax(Syntax s);
uint64_t hash(const uint8_t* data, size_t size);
ParseNode* parseProgram(const TokenList* list);
void parseNodeRemoveChild(ParseNode* parent, uint16_t i);
void parseTreeFree(ParseNode* root);
void parseTreePrint(const ParseNode* root);
Data eval(ParseNode* node);

#endif //_AARDVARK_H
