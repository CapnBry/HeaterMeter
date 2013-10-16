CFLAGS += -Os -I./lmfit -Wall
LDFLAGS += -lm -llua -shared

OBJ = lualmfit.o lmfit/lmmin.o

lmfit: $(OBJ)
	$(CC) $(LDFLAGS) -o lmfit.so $(OBJ)

