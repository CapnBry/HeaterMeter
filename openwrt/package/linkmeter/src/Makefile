CFLAGS +=-Wall -fdata-sections -ffunction-sections -Os
LDFLAGS +=-Wl,--gc-sections
LIBS += -luci

all: hmdude

hmdude: hmdude.o fileio.o bcm2835.o
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

clean:
	rm *.o hmdude
