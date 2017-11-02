#CFLAGS +=-Wall -fdata-sections -ffunction-sections -Os
#LDFLAGS +=-Wl,--gc-sections
#LIBS += -luci

all: ssdp-notify

ssdp-notify: ssdp-notify.o lssdp.o
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

clean:
	rm *.o ssdp-notify
