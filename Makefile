CC := gcc
CFLAGS := -std=c99 -Wall -Wextra -O1
OBJECTS := main.o tokenize.o parse.o eval.o

aardvark: $(OBJECTS)
	$(CC) $(CFLAGS) -o aardvark $(OBJECTS)

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

tokenize.o: tokenize.c
	$(CC) $(CFLAGS) -c tokenize.c

parse.o: parse.c
	$(CC) $(CFLAGS) -c parse.c

eval.o: eval.c
	$(CC) $(CFLAGS) -c eval.c

clean:
	rm -f aardvark $(OBJECTS)
