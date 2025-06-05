#include "aardvark.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdbool.h>

#define KEYWORD_COUNT	9
static const char* keywords[KEYWORD_COUNT] = {
	"do",
	"else",
	"end",
	"fn",
	"if",
	"return",
	"then",
	"var",
	"while",
};

static void _printSyntaxString(const char* s) {
	printf("%s", strchr(s, '_') + 1);
}
#define CASE(x)	case x: _printSyntaxString(#x); return
void printSyntax(Syntax s) {
	switch (s) {
	CASE(TOKEN_IDENTIFIER);
	CASE(TOKEN_INTEGER);
	CASE(TOKEN_STRING);
	CASE(TOKEN_COMMA);
	CASE(TOKEN_L_PAREN);
	CASE(TOKEN_R_PAREN);
	CASE(TOKEN_PLUS);
	CASE(TOKEN_MINUS);
	CASE(TOKEN_MULTIPLY);
	CASE(TOKEN_DIVIDE);
	CASE(TOKEN_ASSIGN);
	CASE(TOKEN_NOT);
	CASE(TOKEN_GREATER);
	CASE(TOKEN_LESS);
	CASE(TOKEN_EQUAL);
	CASE(TOKEN_NOT_EQUAL);
	CASE(TOKEN_GREATER_EQUAL);
	CASE(TOKEN_LESS_EQUAL);
	CASE(TOKEN_DO);
	CASE(TOKEN_ELSE);
	CASE(TOKEN_END);
	CASE(TOKEN_FN);
	CASE(TOKEN_IF);
	CASE(TOKEN_RETURN);
	CASE(TOKEN_THEN);
	CASE(TOKEN_VAR);
	CASE(TOKEN_WHILE);
	CASE(SYNTAX_PROGRAM);
	CASE(SYNTAX_FUNCTION);
	CASE(SYNTAX_BLOCK);
	CASE(SYNTAX_DECLARATION);
	CASE(SYNTAX_ASSIGNMENT);
	CASE(SYNTAX_FUNCTION_CALL);
	CASE(SYNTAX_PARAMETER_LIST);
	CASE(SYNTAX_ARGUMENT_LIST);
	CASE(SYNTAX_RETURN);
	CASE(SYNTAX_IF);
	CASE(SYNTAX_WHILE);
	default:
		fprintf(stderr, "Error: Unknown syntax item %#hhx\n", s);
		exit(EXIT_FAILURE);
	}
}
#undef CASE

static void addToken(TokenList* list, Token t) {
	if (list->tokenCount == list->tokenCapacity) {
		list->tokenCapacity *= 2;
		list->tokens = realloc(list->tokens, list->tokenCapacity * sizeof *list->tokens);
	}
	list->tokens[list->tokenCount++] = t;
}

uint64_t hash(const uint8_t* data, size_t size) {
	uint64_t result = 0;
	for (size_t i = 0; i < size; ++i) {
		result ^= (uint64_t)data[i] << ((i & 0b111) << 3);
	}
	return result;
}

static Token readIdentifierOrKeyword(const char** const chars, const char* const end) {
	Token t = {};
	const char* begin = *chars;
	while (*chars != end) {
		if (!isalnum(**chars) && **chars != '_') {
			break;
		}
		++*chars;
	}
	const size_t length = *chars - begin;
	for (uint8_t i = 0; i < KEYWORD_COUNT; ++i) {
		const size_t keywordLength = strlen(keywords[i]);
		const size_t l = length > keywordLength ? length : keywordLength;
		if (strncmp(begin, keywords[i], l) == 0) {
			t.syntax = TOKEN_DO + i;
			return t;
		}
	}
	t.syntax = TOKEN_IDENTIFIER;
	t.data.identifier = hash((const uint8_t*)begin, length);
	return t;
}

static Token readIntegerLiteral(const char** const chars, const char* const end) {
	int64_t value = 0;
	while (*chars != end) {
		if (!isdigit(**chars)) {
			break;
		}
		value *= 10;
		value += (int64_t)(**chars - '0');
		++*chars;
	}
	Token t = {
		.syntax = TOKEN_INTEGER,
		.data.integerLiteral = value,
	};
	return t;
}

static char _readEscapeSequence(const char** const chars, const char* const end) {
	while (*chars != end) {
		switch (*((*chars)++)) {
		case 'n':
			return '\n';
		case '\\':
			return '\\';
		}
	}
	fprintf(stderr, "Error: Reached end of string literal before end of escape sequence\n");
	exit(EXIT_FAILURE);
}

// NOTE: Supported escape sequences are \\ and \n
static Token readStringLiteral(const char** const chars, const char* const end) {
	Token t = { .syntax = TOKEN_STRING };
	++*chars;
	const char* begin = *chars;
	size_t extra = 0;
	bool escaped = false;
	while (*chars != end) {
		if (**chars == '"' ) {
			break;
		}
		if (**chars == '\\' && !escaped) {
			++extra;
			escaped = true;
		}
		else {
			escaped = false;
		}
		++*chars;
	}
	if (*chars == end) {
		fprintf(stderr, "Error: Reached end of characters before terminating '\"' of string literal\n");
		exit(EXIT_FAILURE);
	}
	const size_t length = *chars - begin;
	const size_t actualLength = length - extra;
	char* const string = malloc(actualLength + 1);
	const char* prev = begin;
	const char* where;
	char* dst = string;
	while ((where = memchr(prev, '\\', *chars - prev)) != NULL) {
		memcpy(dst, prev, where - prev);
		dst += where - prev;
		++where;
		*(dst++) = _readEscapeSequence(&where, *chars);
		prev = where;
	}
	memcpy(dst, prev, *chars - prev);
	string[actualLength] = '\0';
	t.data.stringLiteral = string;
	++*chars;
	return t;
}

static Token readOperator(const char** const chars, const char* const end, Syntax prefix) {
	Token t = { .syntax = prefix };
	++*chars;
	if (*chars == end) {
		return t;
	}
	if (**chars != '=') {
		return t;
	}
	++*chars;
	t.syntax += TOKEN_EQUAL - TOKEN_ASSIGN;
	return t;
}

TokenList tokenize(const char* chars, size_t count) {
	TokenList list = {
		.tokenCapacity = 16,
		.tokenCount = 0,
		.tokens = malloc(16 * sizeof *list.tokens),
	};
	const char* const end = chars + count;
	while (chars < end) {
		Token t = {};
		switch (*chars) {
		case '_':
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
		case 'g':
		case 'h':
		case 'i':
		case 'j':
		case 'k':
		case 'l':
		case 'm':
		case 'n':
		case 'o':
		case 'p':
		case 'q':
		case 'r':
		case 's':
		case 't':
		case 'u':
		case 'v':
		case 'w':
		case 'x':
		case 'y':
		case 'z':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
		case 'G':
		case 'H':
		case 'I':
		case 'J':
		case 'K':
		case 'L':
		case 'M':
		case 'N':
		case 'O':
		case 'P':
		case 'Q':
		case 'R':
		case 'S':
		case 'T':
		case 'U':
		case 'V':
		case 'W':
		case 'X':
		case 'Y':
		case 'Z':
			t = readIdentifierOrKeyword(&chars, end);
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
			t = readIntegerLiteral(&chars, end);
			break;
		case '"':
			t = readStringLiteral(&chars, end);
			break;
		case ',':
			t.syntax = TOKEN_COMMA;
			++chars;
			break;
		case '(':
			t.syntax = TOKEN_L_PAREN;
			++chars;
			break;
		case ')':
			t.syntax = TOKEN_R_PAREN;
			++chars;
			break;
		case '+':
			t.syntax = TOKEN_PLUS;
			++chars;
			break;
		case '-':
			t.syntax = TOKEN_MINUS;
			++chars;
			break;
		case '*':
			t.syntax = TOKEN_MULTIPLY;
			++chars;
			break;
		case '/':
			t.syntax = TOKEN_DIVIDE;
			++chars;
			break;
		case '=':
			t = readOperator(&chars, end, TOKEN_ASSIGN);
			break;
		case '!':
			t = readOperator(&chars, end, TOKEN_NOT);
			break;
		case '>':
			t = readOperator(&chars, end, TOKEN_GREATER);
			break;
		case '<':
			t = readOperator(&chars, end, TOKEN_LESS);
			break;
		case ' ':
		case '\t':
		case '\n':
			++chars;
			break;
		default:
			fprintf(stderr, "Error: Unknown character '%#hhx'\n", *chars);
			exit(EXIT_FAILURE);
		}
		if (t.syntax != SYNTAX_NONE) {
			addToken(&list, t);
		}
	}
	return list;
}
