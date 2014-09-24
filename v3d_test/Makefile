ARM_CC=arm-linux-gnueabihf-gcc

X86_CC=gcc

CLE_AUTOGEN=../cle_autogen/cle_autogen.py

CFLAGS=-g -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-value -c -Wimplicit-function-declaration -fgnu89-inline -std=c99 -Wno-implicit-function-declaration
ARM_CFLAGS=-marm -march=armv6 -mfpu=vfp -mfloat-abi=hard $(CFLAGS) 

ARM_LDFLAGS=-mcpu=arm1176jzf-s -mfloat-abi=hard

CLE_AUTOGEN_NAME=v3d_cl_instr_autogen
AUTOGEN_C=$(CLE_AUTOGEN_NAME).c
AUTOGEN_H=$(CLE_AUTOGEN_NAME).h

SOURCES_C=$(AUTOGEN_C) v3d_test.c v3d_hw.c
ARM_OBJECTS_C=$(SOURCES_C:.c=.c.arm.o)

V3D_TEST_ARM=v3d_test.arm

all: $(AUTOGEN_C) $(AUTOGEN_H) $(SOURCES_C) $(V3D_TEST_ARM) 
clean:
	rm -f $(ARM_OBJECTS_C) $(CLDUMP_ARM) $(AUTOGEN_C) $(AUTOGEN_H)

$(V3D_TEST_ARM): $(ARM_OBJECTS_C)
	$(ARM_CC) $(ARM_LDFLAGS) $(ARM_OBJECTS_C) -o $@

%.c.arm.o: %.c
	$(ARM_CC) $(ARM_CFLAGS) $< -o $@

$(AUTOGEN_C) $(AUTOGEN_H): $(CLE_AUTOGEN)
	$(CLE_AUTOGEN) $(CLE_AUTOGEN_NAME)

