ARM_CC=arm-linux-gnueabihf-gcc

X86_CC=gcc

CLE_AUTOGEN=../cle_autogen/cle_autogen.py

CFLAGS=-g -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-value -c -Wimplicit-function-declaration -fgnu89-inline -std=c99 -Wno-implicit-function-declaration
ARM_CFLAGS=-marm -march=armv6 -mfpu=vfp -mfloat-abi=hard $(CFLAGS) 
X86_CFLAGS=$(CFLAGS)

ARM_LDFLAGS=-mcpu=arm1176jzf-s -mfloat-abi=hard
X86_LDFLAGS=

CLE_AUTOGEN_NAME=v3d_cl_instr_autogen
AUTOGEN_C=$(CLE_AUTOGEN_NAME).c
AUTOGEN_H=$(CLE_AUTOGEN_NAME).h

SOURCES_C=$(AUTOGEN_C) cl_dump.c cl_dis.c qpudis.c

ARM_OBJECTS_C=$(SOURCES_C:.c=.c.arm.o)
X86_OBJECTS_C=$(SOURCES_C:.c=.c.x86.o)

CLDUMP_ARM=cl_dump.arm
CLDUMP_X86=cl_dump.x86

all: $(AUTOGEN_C) $(AUTOGEN_H) $(SOURCES_C) $(CLDUMP_ARM) $(CLDUMP_X86) 

clean:
	rm -f $(ARM_OBJECTS_C) $(X86_OBJECTS_C) $(CLDUMP_ARM) $(CLDUMP_X86) $(AUTOGEN_C) $(AUTOGEN_H)

$(CLDUMP_ARM): $(ARM_OBJECTS_C)
	$(ARM_CC) $(ARM_LDFLAGS) $(ARM_OBJECTS_C) -o $@

$(CLDUMP_X86): $(X86_OBJECTS_C)
	$(X86_CC) $(X86_LDFLAGS) $(X86_OBJECTS_C) -o $@

%.c.arm.o: %.c
	$(ARM_CC) $(ARM_CFLAGS) $< -o $@

%.c.x86.o: %.c
	$(X86_CC) $(X86_CFLAGS) $< -o $@

$(AUTOGEN_C) $(AUTOGEN_H): $(CLE_AUTOGEN)
	$(CLE_AUTOGEN) $(CLE_AUTOGEN_NAME)

