# Copyright (c) 2011 Bart Van Der Meerssche

# Compiler flag to set the C Standard level.
#     c89   = "ANSI" C
#     gnu89 = c89 plus GCC extensions
#     c99   = ISO C99 standard (not yet fully implemented)
#     gnu99 = c99 plus GCC extensions
CSTANDARD = -std=gnu99

all: luad.o
	$(CC) $(LDFLAGS) $(CSTANDARD) -ldl -lm -ldaemon -llua luad.o -o luad

luad.o: luad.c
	$(CC) $(CFLAGS) $(CSTANDARD) -Wall -c luad.c

clean:
	rm *.o luad
