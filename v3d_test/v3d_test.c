#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "v3d_hw.h"
#include "v3d_cl_instr_autogen.h"

#define DMA_MAGIC 0xdd
#define DMA_CMA_SET_SIZE _IOW(DMA_MAGIC, 10, unsigned long)

#define V3D_MEM_SIZE 64 * 1024 * 1024 //64 MB V3D memory
#define BIN_OVERSPILL_PAGES 256 //1 MB Binner overspill memory

int fd_v3d = -1;
int fd_mem = -1;

uint32_t v3d_pa               = -1;
void*    v3d_va               = 0;
void*    v3d_alloced_mem_end  = 0;
void*    v3d_binner_overspill = 0;

struct GPU_BASE gpu_base;

uint32_t clear_colour;
uint32_t mem_mask;
uint32_t rdr_start_offset;

void* simple_alloc(uint32_t pages) {
   if((v3d_alloced_mem_end + (pages << 12)) < (v3d_va + V3D_MEM_SIZE)) {
      void* ret = v3d_alloced_mem_end;
      v3d_alloced_mem_end += (pages << 12);

      return ret;
   }

   return 0;
}

uint32_t virt_to_phys(void* va) {
   return (va - (v3d_va - v3d_pa));
}

typedef union {
   float f;
   uint32_t i;
} floatint_t;

uint32_t float_to_bits(float f) {
   floatint_t fi;
   fi.f = f;
   return fi.i;
}

int startup(void) {
    int mb = mbox_open();

    if(v3d_init(mb, 64 * 1024 * 1024, &gpu_base)) {
        printf("V3D init failed\n");
        return 1;
    }

    v3d_va = gpu_base.mem_base.arm.vptr;
    v3d_pa = gpu_base.mem_base.vc;

   v3d_alloced_mem_end = v3d_va;
   v3d_binner_overspill = simple_alloc(BIN_OVERSPILL_PAGES);
   if(!v3d_binner_overspill) {
      printf("Could not allocate binner overspill memory, closing\n");
      goto fail;
   }

   printf("Binner overspill at v: %p, p: %x\n", v3d_binner_overspill, virt_to_phys(v3d_binner_overspill));

   return 0;
fail:
   v3d_shutdown(&gpu_base);
   return 1;
}

#define TEST_BIN_CL_PAGES 4
#define TEST_RDR_CL_PAGES 4

#define BIN_MEM_SIZE_PAGES 100
#define W_IN_TILES 10
#define H_IN_TILES 10
#define BIN_STATE_MEM_SIZE_PAGES ((W_IN_TILES * H_IN_TILES * 48) >> 12) + 1

#define INITIAL_TILE_CL_SIZE 32

int produce_test_bin(void* start, void** end, void* bin_mem, void* state_bin) {
   volatile void* cur_bin_ins = start;

   printf("Binning memory at virt: %p, phys: %x\n", bin_mem, virt_to_phys(bin_mem));
   printf("Binning state memory at virt: %p, phys: %x\n", state_bin, virt_to_phys(state_bin));

   emit_NOP(&cur_bin_ins);
   emit_STATE_TILE_BINNING_MODE(&cur_bin_ins,
         virt_to_phys(bin_mem),
         BIN_MEM_SIZE_PAGES << 12,
         virt_to_phys(state_bin),
         W_IN_TILES,
         H_IN_TILES,
         0, //multisample
         0, //64-bit colour
         1, //initialise tile state
         0, //initial tile block size
         2, //allocation tile block size
         0  //double buffer
         );

   emit_START_TILE_BINNING(&cur_bin_ins);
   emit_PRIMITIVE_LIST_FORMAT(&cur_bin_ins, 2, 1);

   emit_INCR_SEMAPHORE(&cur_bin_ins);
   emit_FLUSH(&cur_bin_ins);

   *end = cur_bin_ins;

   return 0;
}


int produce_test_rdr(void* start, void** end, void* fb, void* bin_mem) {
   volatile void* cur_rdr_ins = start;
   uint32_t x;
   uint32_t y;

   printf("Framebuffer at virt: %p, phys: %x\n", fb, virt_to_phys(fb));

   emit_STATE_CLEARCOL(&cur_rdr_ins, clear_colour, clear_colour, 0x0, 0x0, 0x0);

   emit_STATE_TILE_RENDERING_MODE(&cur_rdr_ins,
         virt_to_phys(fb),
         W_IN_TILES * 64,
         H_IN_TILES * 64,
         0, //multisample
         0, //64-bit colour
         1, //colour format
         0, //decimate mode
         0, //memory format,
         0, //enable vg mask
         0, //coverage enable
         0, //early-z update direction
         1, //early-z disable
         0, //double buffer
         0  //UNUSED
         );

   emit_STORE_GENERAL(&cur_rdr_ins, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);

   emit_WAIT_SEMAPHORE(&cur_rdr_ins);

   for(y = 0;y < H_IN_TILES;y++) {
      for(x = 0;x < W_IN_TILES;x++) {
         void* tile_sublist;
         emit_STATE_TILE_COORDS(&cur_rdr_ins, x, y);

         emit_PRIMITIVE_LIST_FORMAT(&cur_rdr_ins, 2, 1);
         
         tile_sublist = bin_mem + (x + y * W_IN_TILES) * INITIAL_TILE_CL_SIZE;

         printf("Sublist for tile x: %d y: %d at virt: %p phys: %x\n", x, y, tile_sublist, virt_to_phys(tile_sublist)); 

         emit_BRANCH_SUB(&cur_rdr_ins, virt_to_phys(tile_sublist));

         if(x == W_IN_TILES - 1 && y == H_IN_TILES - 1) {
            emit_STORE_SUBSAMPLE_EOF(&cur_rdr_ins);
         } else {
            emit_STORE_SUBSAMPLE(&cur_rdr_ins);
         }
      }
   }

   *end = cur_rdr_ins;

   return 0;
}

void* bin_cl;
void* rdr_cl;
void* bin_end;
void* rdr_end;
void* fb;

int produce_test_cls() {
   void* bin_mem = simple_alloc(BIN_MEM_SIZE_PAGES);
   void* state_bin = simple_alloc(BIN_STATE_MEM_SIZE_PAGES);

   if(!bin_mem) {
      printf("Failed to allocated binning memory\n");
      return 1;
   }

   if(!state_bin) {
      printf("Failed to allocated binning state memory\n");
      return 1;
   }

   if(produce_test_bin(bin_cl, &bin_end, bin_mem, state_bin)) {
      printf("Bin list creation failed\n");
      return 1;
   }

   if(produce_test_rdr(rdr_cl, &rdr_end, fb, bin_mem)) {
      printf("Render list creation failed\n");
      return 1;
   }

   return 0;
}

void dump_out_fb() {
   FILE* fb_dump;
   fb_dump = fopen("fb.bin", "wb");

   if(fb_dump == NULL) {
    printf("Could not open fb file\n");
    return 0;
   }

   fwrite(fb, 4, H_IN_TILES * 64 * W_IN_TILES * 64, fb_dump);

   fclose(fb_dump);
}

int run_test() {
   bin_cl = simple_alloc(TEST_BIN_CL_PAGES);
   rdr_cl = simple_alloc(TEST_RDR_CL_PAGES);
   fb = simple_alloc(((H_IN_TILES * 64 * W_IN_TILES * 64 * 4) >> 12) + 1);

   memset(fb, 0xFF, H_IN_TILES * 64 * W_IN_TILES * 64 * 4);

   if(!fb) {
      printf("Failled to allocate framebuffer\n");
      return 1;
   }

   if(!bin_cl) {
      printf("Failed to allocated bin CL\n");
      return 1;
   }

   if(!rdr_cl) {
      printf("Failed to allocated rdr CL\n");
      return 1;
   }

   rdr_cl += rdr_start_offset;

   if(produce_test_cls()) {
      printf("CL production failed\n");
      return 1;
   }

   printf("Produced test CLs\nRDR Start v:%p p:%x\n"
         "RDR End v:%p p:%x\n"
         "BIN start v:%p p:%x\n"
         "BIN end v:%p p:%x\n",
         rdr_cl, virt_to_phys(rdr_cl),
         rdr_end, virt_to_phys(rdr_end),
         bin_cl, virt_to_phys(bin_cl),
         bin_end, virt_to_phys(bin_end));

   if(v3d_run_job(&gpu_base, 
           virt_to_phys(bin_cl), virt_to_phys(bin_end), 
           virt_to_phys(rdr_cl), virt_to_phys(rdr_end), 
           virt_to_phys(v3d_binner_overspill), BIN_OVERSPILL_PAGES << 12)) {
        printf("Job failure\n");
        return 1;
   }

   printf("Job finished\n");

   dump_out_fb();

   return 0;
}

int main(int argc, char* argv[]) {
    if(argc != 4) {
        printf("ARGS!\n");
        return 0;
    }

    sscanf(argv[1], "0x%x", &clear_colour);
    sscanf(argv[2], "0x%x", &mem_mask);
    sscanf(argv[3], "0x%x", &rdr_start_offset);

    printf("Clear colour: %08x, mem_mask: %08x, rdr_start_offset: %08x\n", clear_colour, mem_mask, rdr_start_offset);

   if(startup())
      return 1;

   if(run_test());
        printf("Test failed\n");

   v3d_shutdown(&gpu_base);

   return 0;
}
