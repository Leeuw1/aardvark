#include "aardvark.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUFFER_SIZE	128

enum {
	FLAGS_INTERPRET_FILE	= 0x1,
	FLAGS_SHOW_TOKEN_LIST	= 0x2,
	FLAGS_SHOW_SYNTAX_TREE	= 0x4,
};

static void interpret(char* chars, size_t size, uint32_t flags) {
	TokenList list = tokenize(chars, size);
	if (flags & FLAGS_INTERPRET_FILE) {
		free(chars);
	}
	if (flags & FLAGS_SHOW_TOKEN_LIST) {
		printf("Token list:\n");
		if (list.tokenCount == 0) {
			printf("(No tokens)\n");
		}
		else {
			printSyntax(list.tokens[0].syntax);
			for (size_t i = 1; i < list.tokenCount; ++i) {
				putchar(',');
				putchar(' ');
				printSyntax(list.tokens[i].syntax);
			}
			putchar('\n');
		}
		putchar('\n');
	}
	ParseNode* parseTree = parseProgram(&list);
	if (parseTree == NULL) {
		return;
	}
	if (flags & FLAGS_SHOW_SYNTAX_TREE) {
		printf("Parse tree:\n");
		parseTreePrint(parseTree);
		putchar('\n');
	}
	Data result = eval(parseTree);
	switch (result.type) {
	case TYPE_INTEGER:
		printf("%li\n", result.integer);
		break;
	case TYPE_STRING:
		printf("\"%s\"\n", result.string);
		break;
	case TYPE_NONE:
	case TYPE_VOID:
	default:
		break;
	}
	parseTreeFree(parseTree);
	free(parseTree);
}

static uint32_t flag(char c) {
	switch (c) {
	case 't':
		return FLAGS_SHOW_TOKEN_LIST;
	case 's':
		return FLAGS_SHOW_SYNTAX_TREE;
	default:
		fprintf(stderr, "Error: Unknown flag '%c'\n", c);
		exit(EXIT_FAILURE);
	}
}

static uint32_t parseFlags(const char* arg) {
	uint32_t flags = 0;
	for (size_t i = 1; arg[i] != '\0'; ++i) {
		flags |= flag(arg[i]);
	}
	return flags;
}

static int repl(uint32_t flags) {
	printf("aardvark REPL\n");
	char chars[BUFFER_SIZE];
	while (true) {
		printf("> ");
		fflush(stdout);
		ssize_t length = read(STDIN_FILENO, chars, BUFFER_SIZE);
		if (strncmp(chars, "q\n", length) == 0) {
			return EXIT_SUCCESS;
		}
		interpret(chars, length - 1, flags);
	}
}

int main(int argc, const char* argv[]) {
	const char* filepath = NULL;
	uint32_t flags = 0;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--help") == 0) {
			printf("Usage: %s [options] [file]\n", argv[0]);
			printf("Options:\n    -t: Show token list\n    -s: Show syntax tree\n");
			return EXIT_SUCCESS;
		}
		if (argv[i][0] == '-') {
			flags |= parseFlags(argv[i]);
		}
		else {
			filepath = argv[i];
		}
	}
	if (filepath != NULL) {
		int file = open(filepath, O_RDONLY);
		if (file == -1) {
			fprintf(stderr, "Error: Failed to open file '%s'\n", filepath);
			return EXIT_FAILURE;
		}
		struct stat s;
		fstat(file, &s);
		if (!S_ISREG(s.st_mode)) {
			fprintf(stderr, "Error: '%s' is not a file\n", filepath);
			return EXIT_FAILURE;
		}
		const size_t size = s.st_size;
		char* chars = malloc(size);
		read(file, chars, size);
		close(file);
		interpret(chars, size, flags | FLAGS_INTERPRET_FILE);
		return EXIT_SUCCESS;
	}
	return repl(flags);
}
