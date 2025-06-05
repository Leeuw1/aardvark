#include "aardvark.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MAX_GLOBAL_SCOPE_COUNT	16
#define MAX_SCOPE_COUNT			32
#define MAX_FUNCTION_COUNT		32
#define MAX_STACK_COUNT			128

typedef struct Variable	Variable;
struct Variable {
	uint64_t	identifier;
	ssize_t		index;
};

typedef struct Function	Function;
struct Function {
	uint64_t	identifier;
	ParseNode*	node;
};

static Variable globalScope[MAX_GLOBAL_SCOPE_COUNT];
static size_t globalScopeCount = 0;
static Variable scope[MAX_SCOPE_COUNT];
static size_t scopeCount = 0;
static Function functions[MAX_FUNCTION_COUNT];
static size_t functionCount = 0;
static Data stack[MAX_STACK_COUNT];
static size_t stackCount = 0;
static size_t frameStart = 0;

static void stackPush(Data d) {
	assert(stackCount != MAX_STACK_COUNT);
	stack[stackCount++] = d;
}

static void declare(ParseNode* identifier) {
	assert(scopeCount != MAX_SCOPE_COUNT);
	identifier->syntax = RUNTIME_KNOWN_VARIABLE;
	scope[scopeCount].identifier = identifier->data.identifier;
	scope[scopeCount].index = stackCount - frameStart;
	++scopeCount;
}

static void lookupVariable(ParseNode* identifier) {
	for (ssize_t i = scopeCount - 1; i >= 0; --i) {
		if (scope[i].identifier == identifier->data.identifier) {
			identifier->syntax = RUNTIME_KNOWN_VARIABLE;
			identifier->stackIndex = scope[i].index;
			return;
		}
	}
	for (ssize_t i = globalScopeCount - 1; i >= 0; --i) {
		if (globalScope[i].identifier == identifier->data.identifier) {
			identifier->syntax = RUNTIME_KNOWN_GLOBAL_VARIABLE;
			identifier->stackIndex = globalScope[i].index;
			return;
		}
	}
	fprintf(stderr, "Error: Variable not in scope\n");
	exit(EXIT_FAILURE);
}

static Data stdPrint(const ParseNode* argList) {
	Data result = {};
	for (uint16_t i = 0; i < argList->childCount; ++i) {
		if (i > 0) {
			putchar(' ');
		}
		Data arg = eval(&argList->children[i]);
		switch (arg.type) {
		case TYPE_INTEGER:
			printf("%li", arg.integer);
			break;
		case TYPE_STRING:
			printf("%s", arg.string);
			break;
		case TYPE_VOID:
		case TYPE_NONE:
		default:
			printf("None");
			break;
		}
	}
	putchar('\n');
	return result;
}

static void lookupFunction(ParseNode* functionCall) {
	const uint64_t identifier = functionCall->children[0].data.identifier;
	switch (identifier) {
	case 0x746e697270:
		functionCall->syntax = RUNTIME_STANDARD_FUNCTION;
		return;
	}
	for (size_t i = 0; i < functionCount; ++i) {
		if (functions[i].identifier == functionCall->children[0].data.identifier) {
			functionCall->syntax = RUNTIME_KNOWN_FUNCTION;
			functionCall->function = functions[i].node;
			return;
		}
	}
	fprintf(stderr, "Error: Function not found\n");
	exit(EXIT_FAILURE);
}

static Data functionCall(ParseNode* functionCall) {
	const ParseNode* argList = &functionCall->children[1];
	for (int8_t i = argList->childCount - 1; i >= 0; --i) {
		stackPush(eval(&argList->children[i]));
	}
	const ParseNode* function = functionCall->function;
	const ParseNode* paramList = &function->children[1];
	for (uint16_t i = 0; i < paramList->childCount; ++i) {
		assert(scopeCount != MAX_SCOPE_COUNT);
		scope[scopeCount].identifier = paramList->children[i].data.identifier;
		scope[scopeCount].index = -(ssize_t)(i + 1);
		++scopeCount;
	}
	const size_t savedFrameStart = frameStart;
	frameStart = stackCount;
	Data result = eval(&function->children[2]);
	scopeCount -= (size_t)paramList->childCount;
	stackCount = frameStart;
	frameStart = savedFrameStart;
	return result;
}

static Data stdFunctionCall(const ParseNode* functionCall) {
	const uint64_t identifier = functionCall->children[0].data.identifier;
	const ParseNode* argList = &functionCall->children[1];
	switch (identifier) {
	case 0x746e697270:
		return stdPrint(argList);
	default:
		fprintf(stderr, "Error: Unknown standard function\n");
		exit(EXIT_FAILURE);
	}
}

static Data evalProgram(ParseNode* node) {
	Data result = {};
	for (int32_t i = 0; i < node->childCount; ++i) {
		if (node->children[i].syntax == SYNTAX_DECLARATION) {
			assert(globalScopeCount < MAX_GLOBAL_SCOPE_COUNT);
			const ParseNode* declaration = &node->children[i];
			globalScope[globalScopeCount].identifier = declaration->children[0].data.identifier;
			globalScope[globalScopeCount].index = stackCount;
			++globalScopeCount;
			Data initialValue = {};
			if (declaration->childCount == 2) {
				initialValue = eval(&declaration->children[1]);
			}
			stackPush(initialValue);
			parseNodeRemoveChild(node, i--);
		}
		else if (node->children[i].syntax == SYNTAX_FUNCTION) {
			// TODO: For functions we can check whether arguments have duplicate names e.g. fn add(a, a)
			// 		Or just do it when parsing
			assert(functionCount < MAX_FUNCTION_COUNT);
			ParseNode* function = &node->children[i];
			functions[functionCount].identifier = function->children[0].data.identifier;
			functions[functionCount].node = function;
			++functionCount;
		}
	}
	for (uint16_t i = 0; i < node->childCount; ++i) {
		result = eval(&node->children[i]);
		if (result.type != TYPE_NONE) {
			return result;
		}
	}
	return result;
}

static Data evalBlock(const ParseNode* node) {
	Data result = {};
	const size_t savedStackCount = stackCount;
	const size_t savedScopeCount = scopeCount;
	for (uint16_t i = 0; i < node->childCount; ++i) {
		result = eval(&node->children[i]);
		if (result.type != TYPE_NONE) {
			break;
		}
	}
	stackCount = savedStackCount;
	scopeCount = savedScopeCount;
	return result;
}

// NOTE:	When an identifier is on the left of an assignment, we do not eval() it.
// NOTE:	When we eval() a function definition we just register its name.
//			The body is evaluated when the function is called.
Data eval(ParseNode* node) {
	Data result = { .type = TYPE_NONE };
	switch (node->syntax) {
	case SYNTAX_PROGRAM:
		return evalProgram(node);
	case SYNTAX_BLOCK:
		return evalBlock(node);
	case SYNTAX_DECLARATION:
		if (node->childCount == 2) {
			result = eval(&node->children[1]);
		}
		if (node->children[0].syntax == TOKEN_IDENTIFIER) {
			declare(&node->children[0]);
		}
		stackPush(result);
		result.type = TYPE_NONE;
		return result;
	case SYNTAX_FUNCTION:
		return result;
	case SYNTAX_ASSIGNMENT:
		if (node->children[0].syntax == TOKEN_IDENTIFIER) {
			lookupVariable(&node->children[0]);
		}
		stack[(ssize_t)frameStart + node->children[0].stackIndex] = eval(&node->children[1]);
		return result;
	case TOKEN_IDENTIFIER:
		lookupVariable(node);
		__attribute__((fallthrough));
	case RUNTIME_KNOWN_VARIABLE:
		return stack[(ssize_t)frameStart + node->stackIndex];
	case RUNTIME_KNOWN_GLOBAL_VARIABLE:
		// Absolute index
		return stack[node->stackIndex];
	case SYNTAX_RETURN:
		if (node->childCount == 1) {
			return eval(&node->children[0]);
		}
		result.type = TYPE_VOID;
		return result;
	case SYNTAX_FUNCTION_CALL:
		lookupFunction(node);
		return eval(node);
	case RUNTIME_KNOWN_FUNCTION:
		return functionCall(node);
	case RUNTIME_STANDARD_FUNCTION:
		return stdFunctionCall(node);
	case SYNTAX_IF:
		for (uint16_t i = 0; i < node->childCount - 1; i += 2) {
			if (eval(&node->children[i]).integer) {
				return eval(&node->children[i + 1]);
			}
		}
		// Odd number of nodes indicates a final 'else' block
		if (node->childCount & 1) {
			return eval(&node->children[node->childCount - 1]);
		}
		return result;
	case SYNTAX_WHILE:
		while (eval(&node->children[0]).integer) {
			result = eval(&node->children[1]);
			if (result.type != TYPE_NONE) {
				return result;
			}
		}
		return result;
	case TOKEN_INTEGER:
		result.type = TYPE_INTEGER;
		result.integer = node->data.integerLiteral;
		return result;
	case TOKEN_STRING:
		result.type = TYPE_STRING;
		result.string = node->data.stringLiteral;
		return result;
	case TOKEN_PLUS:
		result.type = TYPE_INTEGER;
		result.integer = eval(&node->children[0]).integer + eval(&node->children[1]).integer;
		return result;
	case TOKEN_MINUS:
		result.type = TYPE_INTEGER;
		result.integer = eval(&node->children[0]).integer - eval(&node->children[1]).integer;
		return result;
	case TOKEN_MULTIPLY:
		result.type = TYPE_INTEGER;
		result.integer = eval(&node->children[0]).integer * eval(&node->children[1]).integer;
		return result;
	case TOKEN_DIVIDE:
		result.type = TYPE_INTEGER;
		result.integer = eval(&node->children[0]).integer / eval(&node->children[1]).integer;
		return result;
	case TOKEN_EQUAL:
		result.type = TYPE_INTEGER;
		result.integer = eval(&node->children[0]).integer == eval(&node->children[1]).integer;
		return result;
	case TOKEN_NOT_EQUAL:
		result.type = TYPE_INTEGER;
		result.integer = eval(&node->children[0]).integer != eval(&node->children[1]).integer;
		return result;
	case TOKEN_GREATER:
		result.type = TYPE_INTEGER;
		result.integer = eval(&node->children[0]).integer > eval(&node->children[1]).integer;
		return result;
	case TOKEN_LESS:
		result.type = TYPE_INTEGER;
		result.integer = eval(&node->children[0]).integer < eval(&node->children[1]).integer;
		return result;
	case TOKEN_GREATER_EQUAL:
		result.type = TYPE_INTEGER;
		result.integer = eval(&node->children[0]).integer >= eval(&node->children[1]).integer;
		return result;
	case TOKEN_LESS_EQUAL:
		result.type = TYPE_INTEGER;
		result.integer = eval(&node->children[0]).integer <= eval(&node->children[1]).integer;
		return result;
	case TOKEN_NOT:
		result.type = TYPE_INTEGER;
		result.integer = !eval(&node->children[0]).integer;
		return result;
	default:
		fprintf(stderr, "Error: Invalid syntax item for eval()\n");
		exit(EXIT_FAILURE);
	}
}
