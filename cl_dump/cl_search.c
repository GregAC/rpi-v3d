#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>

#include "v3d_cl_instr_autogen.h"
#include "cl_dump.h"

uint8_t seq_skip[] = {14, 0, 0, 0, 0};
uint8_t search_seq[] = {112, 2 << 5 | 1 << 2, 6, 56, 2 | 1 << 4};
uint8_t seq_length = 5;

int do_search(char* start_addr_str, char* size_str) {
    uint32_t start_addr;
    uint32_t size;
    uint8_t* search_area; 

    if(sscanf(start_addr_str, "0x%x", &start_addr) != 1) {
        fprintf(stderr, "Search start address must be of the form 0x1234abcd\n");
        return 1;
    }

    if(sscanf(size_str, "%d", &size) != 1) {
        fprintf(stderr, "Search size must be a decimal number\n");
        return 1;
    }

    search_area = map_area(start_addr, size);

    if(!search_area) {
        fprintf(stderr, "Could not map search area\n");
        return 1;
    }

    uint8_t*  current   = search_area;
    uint8_t*  area_end  = search_area + size;
    uint8_t*  seq_begin = 0;
    uint32_t  seq_cur   = 0;
    uint32_t  found     = 0;

    while(current < area_end) {
        if(*current == search_seq[seq_cur]) {
            if(!seq_begin)
                seq_begin = current;

            current += seq_skip[seq_cur];
            ++seq_cur;
        } else if(seq_begin){
            current = seq_begin;
            seq_begin = 0;
            seq_cur = 0;
        }

        if(seq_cur == seq_length) {
            found = 1;
            printf("Found a bin list beginning at %x\n", (uint32_t)((seq_begin - search_area) + start_addr));

            seq_begin = 0;
            seq_cur = 0;
        }

        current++;
    }

    if(!found) {
        printf("No bin lists found\n");
    }

    unmap_area(search_area, size);

    return 0;
}

