#ifndef __CL_DUMP_H__
#define __CL_DUMP_H__

//#define CL_DUMP_DEBUG

#include <stdint.h>

int do_dis(char* start_addr_str, char* end_addr_str);
void* map_area(uint32_t addr, uint32_t size);
void unmap_area(void* addr, uint32_t size);

#endif

