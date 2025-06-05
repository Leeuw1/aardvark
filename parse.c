#include "aardvark.h"

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

// NOTE:	To avoid exessive code repetition in parsing functions, these macros have been created
//			- Some macros have NO_POP and MERGE variants
//				- NO_POP means do not call parseNodePopChild()
//				- MERGE means call parseNodeMerge()
//			- STAR and QUESTION refer to the BNF syntax * and ?
//				- STAR means match 0 or more occurrences
//				- QUESTION means match 0 or 1 occurrences, can also be used for functions that cannot fail
// NOTE:	Parsing functions must have the same names for arguments 't', 'parent', and 'end'
#define SAVE()						const Token* _savedT = *t;\
									ParseNode* _savedParent = parent;\
									const uint16_t _savedChildCount = parent->childCount;\
									(void)_savedT; (void)_savedParent; (void)_savedChildCount
#define PUSH(s)						parent = parseNodePushChild(parent, s)
#define FAIL()						parseNodePopChild(_savedParent);\
									FAIL_NO_POP()
#define FAIL_NO_POP()				*t = _savedT;\
									return false
#define SUCCEED()					return true
#define FAIL_IF_NOT(func)			if (!func(t, end, parent)) { FAIL(); }
#define SUCCEED_IF(func)			if (func(t, end, parent)) { SUCCEED(); }
#define STAR(func)					while (func(t, end, parent)) {}
#define QUESTION(func)				func(t, end, parent)
#define FAIL_IF_NOT_T(tok)			if (!parseToken(t, end, parent, tok)) { FAIL(); }
#define SUCCEED_IF_T(tok)			if (parseToken(t, end, parent, tok)) { SUCCEED(); }
#define FAIL_IF_NOT_NO_POP(func)	if (!func(t, end, parent)) { FAIL_NO_POP(); }
#define FAIL_IF_NOT_T_NO_POP(tok)	if (!parseToken(t, end, parent, tok)) { FAIL_NO_POP(); }
#define SUCCEED_MERGE()				parseNodeMerge(&_savedParent->children[0]); SUCCEED()
#define SUCCEED_IF_MERGE(func)		if (func(t, end, parent)) { SUCCEED_MERGE(); }
#define SUCCEED_IF_T_MERGE(tok)		if (parseToken(t, end, parent, tok)) { SUCCEED_MERGE(); }

static bool parseToken(const Token** t, const Token* const end, ParseNode* parent, Syntax targetToken);
static bool parseComponent(const Token** t, const Token* const end, ParseNode* parent);
static bool parseFunction(const Token** t, const Token* const end, ParseNode* parent);
static bool parseBlock(const Token** t, const Token* const end, ParseNode* parent);
static bool parseLine(const Token** t, const Token* const end, ParseNode* parent);
static bool parseDeclaration(const Token** t, const Token* const end, ParseNode* parent);
static bool parseAssignment(const Token** t, const Token* const end, ParseNode* parent);
static bool parseFunctionCall(const Token** t, const Token* const end, ParseNode* parent);
static bool parseParameterList(const Token** t, const Token* const end, ParseNode* parent);
static bool parseArgumentList(const Token** t, const Token* const end, ParseNode* parent);
static bool parseReturn(const Token** t, const Token* const end, ParseNode* parent);
static bool parseControlStructure(const Token** t, const Token* const end, ParseNode* parent);
static bool parseIf(const Token** t, const Token* const end, ParseNode* parent);
static bool parseWhile(const Token** t, const Token* const end, ParseNode* parent);
static bool parseExpression(const Token** t, const Token* const end, ParseNode* parent);
static bool parseUnaryExpression(const Token** t, const Token* const end, ParseNode* parent);
static bool parsePrimaryExpression(const Token** t, const Token* const end, ParseNode* parent);

static void parseNodeCreate(ParseNode* node, Syntax s) {
	memset(node, 0, sizeof *node);
	node->syntax = s;
	node->childCapacity = 2;
	node->children = malloc(2 * sizeof *node->children);
}

static ParseNode* parseNodePushChild(ParseNode* parent, Syntax s) {
	assert(parent->childCount < UINT16_MAX);
	if (parent->childCount == parent->childCapacity) {
		assert(!(parent->childCapacity & 0x8000));
		parent->childCapacity *= 2;
		parent->children = realloc(parent->children, (size_t)parent->childCapacity * sizeof *parent->children);
		assert(parent->children != NULL);
	}
	ParseNode* child = &parent->children[parent->childCount++];
	parseNodeCreate(child, s);
	return child;
}

static void _tryShrink(ParseNode* parent) {
	if (parent->childCapacity < 4 || parent->childCapacity < parent->childCount * 3) {
		return;
	}
	parent->childCapacity /= 2;
	parent->children = realloc(parent->children, parent->childCapacity * sizeof *parent->children);
}

static void parseNodePopChild(ParseNode* parent) {
	assert(parent->childCount != 0);
	--parent->childCount;
	parseTreeFree(&parent->children[parent->childCount]);
	_tryShrink(parent);
}

void parseNodeRemoveChild(ParseNode* parent, uint16_t i) {
	assert(i < parent->childCount);
	--parent->childCount;
	parseTreeFree(&parent->children[i]);
	memmove(parent->children + i, parent->children + i + 1, (parent->childCount - i) * sizeof *parent->children);
	_tryShrink(parent);
}

static void parseNodeMerge(ParseNode* parent) {
	assert(parent->childCount == 1);
	void* temp = parent->children;
	*parent = parent->children[0];
	free(temp);
}

void parseTreeFree(ParseNode* root) {
	if (root->syntax == TOKEN_STRING) {
		free((void*)root->data.stringLiteral);
	}
	for (uint16_t i = 0; i < root->childCount; ++i) {
		parseTreeFree(&root->children[i]);
	}
	free(root->children);
}

ParseNode* parseProgram(const TokenList* list) {
	const Token* t = list->tokens;
	const Token* const end = list->tokens + list->tokenCount;
	ParseNode* root = malloc(sizeof *root);
	parseNodeCreate(root, SYNTAX_PROGRAM);
	while (parseComponent(&t, end, root)) {
	}
	free(list->tokens);
	if (t != end) {
		fprintf(stderr, "Error: Did not parse all tokens\n");
		parseTreeFree(root);
		free(root);
		return NULL;
	}
	return root;
}

static void _parseTreePrint(const ParseNode* root, int depth) {
	for (int i = 0; i < depth; ++i) {
		putchar(' ');
		putchar(' ');
	}
	printSyntax(root->syntax);
	if (root->syntax == TOKEN_INTEGER) {
		printf(" %li", root->data.integerLiteral);
	}
	else if (root->syntax == TOKEN_STRING) {
		printf(" \"%s\"", root->data.stringLiteral);
	}
	putchar('\n');
	for (uint16_t i = 0; i < root->childCount; ++i) {
		_parseTreePrint(&root->children[i], depth + 1);
	}
}

void parseTreePrint(const ParseNode* root) {
	_parseTreePrint(root, 0);
}

bool parseComponent(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	SUCCEED_IF(parseFunction);
	SUCCEED_IF(parseLine);
	SUCCEED_IF(parseControlStructure);
	FAIL_NO_POP();
}

bool parseFunction(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	PUSH(SYNTAX_FUNCTION);
	FAIL_IF_NOT_T(TOKEN_FN);
	FAIL_IF_NOT_T(TOKEN_IDENTIFIER);
	FAIL_IF_NOT_T(TOKEN_L_PAREN);
	QUESTION(parseParameterList);
	FAIL_IF_NOT_T(TOKEN_R_PAREN);
	QUESTION(parseBlock);
	FAIL_IF_NOT_T(TOKEN_END);
	SUCCEED();
}

bool _parseInnerComponent(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	SUCCEED_IF(parseLine);
	SUCCEED_IF(parseControlStructure);
	FAIL_NO_POP();
}

bool parseBlock(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	PUSH(SYNTAX_BLOCK);
	STAR(_parseInnerComponent);
	SUCCEED();
}

bool parseLine(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	SUCCEED_IF(parseDeclaration);
	SUCCEED_IF(parseAssignment);
	SUCCEED_IF(parseFunctionCall);
	SUCCEED_IF(parseReturn);
	FAIL_NO_POP();
}

static bool _parseAssignExpression(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	FAIL_IF_NOT_T_NO_POP(TOKEN_ASSIGN);
	FAIL_IF_NOT_NO_POP(parseExpression);
	SUCCEED();
}

bool parseDeclaration(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	PUSH(SYNTAX_DECLARATION);
	FAIL_IF_NOT_T(TOKEN_VAR);
	FAIL_IF_NOT_T(TOKEN_IDENTIFIER);
	QUESTION(_parseAssignExpression);
	SUCCEED();
}

bool parseAssignment(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	PUSH(SYNTAX_ASSIGNMENT);
	FAIL_IF_NOT_T(TOKEN_IDENTIFIER);
	FAIL_IF_NOT_T(TOKEN_ASSIGN);
	FAIL_IF_NOT(parseExpression);
	SUCCEED();
}

bool parseFunctionCall(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	PUSH(SYNTAX_FUNCTION_CALL);
	FAIL_IF_NOT_T(TOKEN_IDENTIFIER);
	FAIL_IF_NOT_T(TOKEN_L_PAREN);
	QUESTION(parseArgumentList);
	FAIL_IF_NOT_T(TOKEN_R_PAREN);
	SUCCEED();
}

static bool _parseCommaIdentifier(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	FAIL_IF_NOT_T_NO_POP(TOKEN_COMMA);
	FAIL_IF_NOT_T_NO_POP(TOKEN_IDENTIFIER);
	SUCCEED();
}

bool parseParameterList(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	PUSH(SYNTAX_PARAMETER_LIST);
	if (!parseToken(t, end, parent, TOKEN_IDENTIFIER)) {
		SUCCEED();
	}
	STAR(_parseCommaIdentifier);
	SUCCEED();
}

static bool _parseCommaExpression(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	FAIL_IF_NOT_T_NO_POP(TOKEN_COMMA);
	FAIL_IF_NOT_NO_POP(parseExpression);
	SUCCEED();
}

bool parseArgumentList(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	PUSH(SYNTAX_ARGUMENT_LIST);
	if (!parseExpression(t, end, parent)) {
		SUCCEED();
	}
	STAR(_parseCommaExpression);
	SUCCEED();
}

bool parseReturn(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	PUSH(SYNTAX_RETURN);
	FAIL_IF_NOT_T(TOKEN_RETURN);
	QUESTION(parseExpression);
	SUCCEED();
}

bool parseControlStructure(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	SUCCEED_IF(parseIf);
	SUCCEED_IF(parseWhile);
	FAIL_NO_POP();
}

static bool _parseElseIf(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	FAIL_IF_NOT_T_NO_POP(TOKEN_ELSE);
	FAIL_IF_NOT_T_NO_POP(TOKEN_IF);
	FAIL_IF_NOT_NO_POP(parseExpression);
	FAIL_IF_NOT_T_NO_POP(TOKEN_THEN);
	QUESTION(parseBlock);
	SUCCEED();
}

static bool _parseElseBlock(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	FAIL_IF_NOT_T_NO_POP(TOKEN_ELSE);
	QUESTION(parseBlock);
	SUCCEED();
}

bool parseIf(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	PUSH(SYNTAX_IF);
	FAIL_IF_NOT_T(TOKEN_IF);
	FAIL_IF_NOT(parseExpression);
	FAIL_IF_NOT_T(TOKEN_THEN);
	QUESTION(parseBlock);
	STAR(_parseElseIf);
	QUESTION(_parseElseBlock);
	FAIL_IF_NOT_T(TOKEN_END);
	SUCCEED();
}

bool parseWhile(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	PUSH(SYNTAX_WHILE);
	FAIL_IF_NOT_T(TOKEN_WHILE);
	FAIL_IF_NOT(parseExpression);
	FAIL_IF_NOT_T(TOKEN_DO);
	QUESTION(parseBlock);
	FAIL_IF_NOT_T(TOKEN_END);
	SUCCEED();
}

static bool _parseParensExpression(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	FAIL_IF_NOT_T_NO_POP(TOKEN_L_PAREN);
	FAIL_IF_NOT_NO_POP(parseExpression);
	FAIL_IF_NOT_T_NO_POP(TOKEN_R_PAREN);
	SUCCEED();
}

// Returns -1 if s is not a binary operator
static int _precedence(Syntax s) {
	switch (s) { 
	case TOKEN_EQUAL:
	case TOKEN_NOT_EQUAL:
	case TOKEN_GREATER:
	case TOKEN_LESS:
	case TOKEN_GREATER_EQUAL:
	case TOKEN_LESS_EQUAL:
		return 0;
	case TOKEN_PLUS:
	case TOKEN_MINUS:
		return 1;
	case TOKEN_MULTIPLY:
	case TOKEN_DIVIDE:
		return 2;
	default:
		return -1;
	}
}

bool parseExpression(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	PUSH(SYNTAX_NONE);
	FAIL_IF_NOT(parseUnaryExpression);
	int prevPrecedence = 0;
	int iteration = 0;
	while (true) {
		if (*t == end) {
			break;
		}
		Syntax s = (*t)->syntax;
		const int precedence = _precedence(s);
		if (precedence == -1) {
			break;
		}
		++*t;
		if (iteration > 0 && precedence <= prevPrecedence) {
			parseNodeMerge(parent);
			ParseNode* node = &_savedParent->children[_savedParent->childCount - 1];
			while (_precedence(node->syntax) <= _precedence(node->children[node->childCount - 1].syntax)) {
				node = &node->children[node->childCount - 1];
			}
			ParseNode new;
			parseNodeCreate(&new, s);
			new.childCount = 1;
			new.children[0] = *node;
			*node = new;
			parent = parseNodePushChild(node, SYNTAX_NONE);
		}
		else {
			parent->syntax = s;
			parent = parseNodePushChild(parent, SYNTAX_NONE);
		}
		if (!parseUnaryExpression(t, end, parent)) {
			FAIL();
		}
		prevPrecedence = precedence;
		++iteration;
	}
	parseNodeMerge(parent);
	SUCCEED();
}

bool parseUnaryExpression(const Token** t, const Token* const end, ParseNode* parent) {
	if (*t == end) {
		return false;
	}
	SAVE();
	switch ((*t)->syntax) {
	case TOKEN_NOT:
		PUSH((*t)->syntax);
		++*t;
		break;
	default:
		FAIL_IF_NOT_NO_POP(parsePrimaryExpression);
		SUCCEED();
	}
	SUCCEED_IF(parsePrimaryExpression);
	SUCCEED_IF(parseUnaryExpression);
	FAIL();
}

bool parsePrimaryExpression(const Token** t, const Token* const end, ParseNode* parent) {
	SAVE();
	SUCCEED_IF(parseFunctionCall);
	SUCCEED_IF(_parseParensExpression);
	SUCCEED_IF_T(TOKEN_IDENTIFIER);
	SUCCEED_IF_T(TOKEN_INTEGER);
	SUCCEED_IF_T(TOKEN_STRING);
	FAIL_NO_POP();
}

bool parseToken(const Token** t, const Token* const end, ParseNode* parent, Syntax targetToken) {
	if (*t == end) {
		return false;
	}
	if ((*t)->syntax != targetToken) {
		return false;
	}
	if (targetToken == TOKEN_IDENTIFIER || targetToken == TOKEN_INTEGER || targetToken == TOKEN_STRING) {
		parseNodePushChild(parent, targetToken)->data = (*t)->data;
	}
	++*t;
	return true;
}
