/*
 * cl_dump - A program for dumping and examining V3D control lists and related
 * structures and buffers
 *
 * Written by Greg Chadwick (mail@gregchadwick.co.uk)
 */


#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "cl_dump.h"

static FILE* fd_mem;
static uint32_t mem_offset;

int startup(void) {
   fd_mem = fopen("/dev/mem", "r+");

   if(fd_mem == 0) {
      fprintf(stderr, "Could not open /dev/mem!\nReported: %s\n", strerror(errno));
      return 1;
   }

   mem_offset = 0;

   return 0;
}

void* map_area(uint32_t addr, uint32_t size) {
   void* va;

   uint32_t page_offset;
   uint32_t page_addr;

   page_addr = addr & 0xFFFFF000;
   page_offset = addr - page_addr;

#ifdef CL_DUMP_DEBUG
   printf("Mapping area: %08x of size %d bytes, page addr: %08x modified size: %d\n", addr, size, page_addr, size + page_offset);
#endif

   va = mmap(0, size + page_offset, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(fd_mem), page_addr - mem_offset);
   if(va == -1) {
      fprintf(stderr, "Mapping of V3D physical memory to virtual failed!\nReported: %s\n", strerror(errno));
      return 0;
   }

   return va + page_offset;
}

void unmap_area(void* addr, uint32_t size) {
   uint32_t page_offset;
   void*    page_addr;

   page_addr = (void*)((uint32_t)addr & 0xFFFFF000);
   page_offset = addr - page_addr;

#ifdef CL_DUMP_DEBUG
   printf("Unmapping area: %p of size %d bytes, page addr: %p modified size: %d\n", addr, size, page_addr, size + page_offset);
#endif

   munmap(page_addr, size + page_offset);
}

int do_dump(char* out_filename, char* addr_str, char* size_str) {
   FILE* out_file;
   void* area;
   uint32_t addr;
   uint32_t size;

   out_file = fopen(out_filename, "wb");
   if(!out_file) {
      fprintf(stderr, "Couldn't open out file\n");
      return 1;
   }

   if(sscanf(addr_str, "0x%x", &addr) != 1) {
      fprintf(stderr, "Address must be of form 0x1234ABCD\n");
      goto fail;
   }

   if(sscanf(size_str, "%d", &size) != 1) {
      fprintf(stderr, "Size must be decimal integer\n");
      goto fail;
   }

   area = map_area(addr, size);

   if(!area) {
      fprintf(stderr, "Failed to map memory\n");
      goto fail;
   }

   if(fwrite(area, 1, size, out_file) != size) {
      fprintf(stderr, "Failed to write out everything");
      goto fail;
   }

   fclose(out_file);
   unmap_area(area, size);

   return 0;
fail:
   if(out_file)
      fclose(out_file);
   if(area)
      unmap_area(area, size);

   return 1;
}

void print_usage(char* argv0) {
   printf("Usage %s cmd\n"
   "cmd one of:\n"
   "\tdump phys_addr size out_file - Dumps raw memory to out_file\n"
   "\tdis cl_start cl_end - Disassembles CL bytes betweeen given addresses\n", argv0);
}

int main(int argc, char* argv[]) {
   if(argc > 5 || argc < 4) {
      print_usage(argv[0]);
      return 1;
   }

   if(startup())
      return 1;

   if(strcmp(argv[1], "dump") == 0) {
      if(argc != 5) {
         print_usage(argv[0]);
         return 1;
      }

      if(do_dump(argv[4], argv[2], argv[3]))
         return 1;

      return 0;
   } else if(strcmp(argv[1], "dis") == 0) {
      if(argc != 4) {
         print_usage(argv[0]);
         return 1;
      }

      if(do_dis(argv[2], argv[3]))
         return 1;

      return 0;
   } else {
      fprintf(stderr, "Invalid command %s\n", argv[1]);
      return 1;
   }

   return 0;
}

