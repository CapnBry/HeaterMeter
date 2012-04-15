TARGET = heatermeter
MCU = atmega328p
F_CPU = 16000000
VARIANT=standard

TOOLROOT = $(ARDUINO_PATH)/hardware/tools
GCCROOT = $(TOOLROOT)/avr/bin
ARD_CORE=$(ARDUINO_PATH)/hardware/arduino/cores/arduino
ARD_VARIANT=$(ARDUINO_PATH)/hardware/arduino/variants/$(VARIANT)

CC = $(GCCROOT)/avr-gcc
CXX = $(GCCROOT)/avr-g++
OBJCOPY = $(GCCROOT)/avr-objcopy
OBJDUMP = $(GCCROOT)/avr-objdump
SIZE = $(GCCROOT)/avr-size

ARD_LIBRARIES=../libraries/jeelib ../libraries/ShiftRegLCD 

OPT = s
CSTANDARD = -std=gnu99
CDEFS = -DF_CPU=$(F_CPU)UL
CPPDEFS = -DF_CPU=$(F_CPU)UL

CFLAGS = -g
CFLAGS += $(CDEFS)
CFLAGS += -O$(OPT)
CFLAGS += -funsigned-char
CFLAGS += -funsigned-bitfields
CFLAGS += -fpack-struct
CFLAGS += -fshort-enums
CFLAGS += -Wall
CFLAGS += -Wstrict-prototypes
CFLAGS += $(CSTANDARD)
CFLAGS += -I$(ARD_CORE) -I$(ARD_VARIANT)
CFLAGS += $(patsubst %,-I%,$(ARD_LIBRARIES))

CPPFLAGS = -g$(DEBUG)
CPPFLAGS += $(CPPDEFS)
CPPFLAGS += -O$(OPT)
CPPFLAGS += -funsigned-char
CPPFLAGS += -funsigned-bitfields
CPPFLAGS += -fpack-struct
CPPFLAGS += -fshort-enums
CPPFLAGS += -fno-exceptions
CPPFLAGS += -Wall
CPPFLAGS += -Wundef
CPPFLAGS += -I$(ARD_CORE) -I$(ARD_VARIANT)
CPPFLAGS += $(patsubst %,-I%,$(ARD_LIBRARIES))

PRINTF_LIB = 
SCANF_LIB = 
MATH_LIB = -lm

LDFLAGS = -Wl,-Map=$(TARGET).map,--cref
LDFLAGS += $(EXTMEMOPTS)
#LDFLAGS += $(patsubst %,-L%,$(EXTRALIBDIRS))
LDFLAGS += $(PRINTF_LIB) $(SCANF_LIB) $(MATH_LIB)

GENDEPFLAGS = -MMD -MP -MF .dep/$(@F).d
ALL_CFLAGS = -mmcu=$(MCU) -I. $(CFLAGS) $(GENDEPFLAGS)
ALL_CPPFLAGS = -mmcu=$(MCU) -I. -x c++ $(CPPFLAGS) $(GENDEPFLAGS)

OBJDIR = .obj

VPATH
SRC = $(wildcard *.c)
CPPSRC = $(wildcard *.cpp)
OBJ = $(SRC:%.c=$(OBJDIR)/%.o) $(CPPSRC:%.cpp=$(OBJDIR)/%.o)

all: build

build: elf hex

elf: $(TARGET).elf
hex: $(TARGET).hex

$(OBJDIR)/%.o : %.c
	$(CC) -c $(ALL_CFLAGS) $< -o $@ 

$(OBJDIR)/%.o : %.cpp
	$(CC) -c $(ALL_CPPFLAGS) $< -o $@ 

%.elf: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)
	$(SIZE) $@

.PHONY: clean
clean:
	rm -rf $(OBJDIR)
	rm -rf .dep

$(shell mkdir $(OBJDIR))
-include $(shell mkdir .dep) $(wildcard .dep/*)
