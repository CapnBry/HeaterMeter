CFLAGS +=-Wall
LIBS += -luci

all: hmdude

hmdude: hmdude.o fileio.o
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

clean:
	rm *.o hmdude
