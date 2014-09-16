#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "v3d_cl_instr_autogen.h"

FILE* fd_mem;

int startup(void) {
   fd_mem = fopen("/dev/mem", "r+");

   if(fd_mem == 0) {
      fprintf(stderr, "Could not open /dev/mem!\nReported: %s\n", strerror(errno));
      return 1;
   }

   return 0;
}

void* map_area(uint32_t addr, uint32_t size) {
   void* va;

   printf("Mapping area: %08X of size %d bytes\n", addr, size);

   va = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(fd_mem), addr);
   if(va == -1) {
      fprintf(stderr, "Mapping of V3D physical memory to virtual failed!\nReported: %s\n", strerror(errno));
      return 0;
   }

   return va;
}

//TODO: if end address is actually inside an instruction this may cause a segmentation error
int do_dis(char* start_addr_str, char* end_addr_str) {
   uint32_t start_addr;
   uint32_t end_addr;
   void* cl_start;
   void* cl_end;
   void* cur_ins;

   uint32_t page_offset;
   uint32_t start_page_addr;

   if(sscanf(start_addr_str, "0x%x", &start_addr) != 1) {
      fprintf(stderr, "Addresses must be of form 0x1234ABCD\n");
      return 1;
   }

   if(sscanf(end_addr_str, "0x%x", &end_addr) != 1) {
      fprintf(stderr, "Addresses must be of form 0x1234ABCD\n");
      return 1;
   }

   printf("Disassembling CL start: %08x end: %08X\n", start_addr, end_addr);

   start_page_addr = (start_addr & 0xFFFFF000);
   page_offset = start_addr - start_page_addr;

   cl_start = map_area(start_page_addr, (end_addr - start_addr) + page_offset);
   if(!cl_start) {
      fprintf(stderr, "Failed to map CL memory\n");
      return 1;
   }

   cl_start += page_offset;
   cur_ins = cl_start;

   cl_end = (end_addr - start_addr) + cl_start;

   while(cur_ins < cl_end) {
      void* next_ins;

      printf("%08x: ", (cur_ins - cl_start) + start_addr);
      if(disassemble_instr(cur_ins, stdout)) {
         printf("INVALID OPCODE (%d)\n", (uint32_t)(*(uint8_t*)cur_ins));
         cur_ins++;
         continue;
      }

      next_ins = calc_next_ins(cur_ins);
      if(next_ins == 0) {
         fprintf(stderr, "Could not calculate next address at %08x\n", (cur_ins - cl_start) + start_addr);
         return 1;
      }

      cur_ins = next_ins;
   }

   return 0;
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

   return 0;
fail:
   if(out_file)
      fclose(out_file);
   if(area)
      munmap(area, size);

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

      if(do_dump(argv[2], argv[3], argv[4]))
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

