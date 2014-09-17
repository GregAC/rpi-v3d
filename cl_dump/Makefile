CC=arm-linux-gnueabihf-gcc
CXX=arm-linux-gnueabihf-g++
CLE_AUTOGEN=../cle_autogen/cle_autogen.py

CPPFLAGS=-g -marm -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-value -march=armv6 -mfpu=vfp -mfloat-abi=hard
CFLAGS=$(CPPFLAGS) -c -Wimplicit-function-declaration -fgnu89-inline -std=c99 -Wno-implicit-function-declaration
LDFLAGS=-mcpu=arm1176jzf-s -mfloat-abi=hard

CLE_AUTOGEN_NAME=v3d_cl_instr_autogen
AUTOGEN_C=$(CLE_AUTOGEN_NAME).c
AUTOGEN_H=$(CLE_AUTOGEN_NAME).h

SOURCES_C=$(AUTOGEN_C) cl_dump.c cl_dis.c

OBJECTS_C=$(SOURCES_C:.c=.c.o)

V3DTEST=cl_dump.arm

all: $(AUTOGEN_C) $(AUTOGEN_H) $(SOURCES_C) $(V3DTEST) 

clean:
	rm -f $(OBJECTS_C) $(V3DTEST) $(AUTOGEN_C) $(AUTOGEN_H)

$(V3DTEST): $(OBJECTS_C)
	$(CC) $(LDFLAGS) $(OBJECTS_C) -o $@

%.c.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

$(AUTOGEN_C) $(AUTOGEN_H): $(CLE_AUTOGEN)
	$(CLE_AUTOGEN) $(CLE_AUTOGEN_NAME)

