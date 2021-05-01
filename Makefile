CC=gcc
CFLAGS=-I. -lm -pthread
DEPS = plagiarism.h
OBJ = plagiarism.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

detector: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f detector *.o

.PHONY: all

all: $(OBJ)
	$(CC) -o detector plagiarism.o $(CFLAGS)
