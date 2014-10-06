/*
 * cl_dis.c - Functions for disassembling V3D control lists and related
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
#include <assert.h>
#include <string.h>

#include "v3d_cl_instr_autogen.h"
#include "cl_dump.h"
#include "qpudis.h"

//When disassembling a CL if we don't have an end address we disassemble
//til we hit a BRANCH (not sub-list branch) or RETURN.  If we've got a 
//bad CL we may end up disassembling junk for a long time.  This puts a limit
//on how large we let a CL get before giving up.
#define MAX_CL_SIZE 512 * 1024 //512 kb

typedef struct {
   uint32_t start_address;
   uint32_t end_address;
   void*    cl_start;
   void*    cl_end;
   void*    cur_ins;
   uint32_t current_area_size;
} dis_state_t;

#define BUF_TYPE_NONE              0
#define BUF_TYPE_CL                1
#define BUF_TYPE_GL_SHADER_REC     2
#define BUF_TYPE_GL_SHADER_REC_EXT 3
#define BUF_TYPE_QPU_PROG          4
#define BUF_TYPE_NV_SHADER_REC     5

typedef struct v3d_buf {
   struct v3d_buf* next;

   uint32_t buf_type;
   uint32_t buf_start;
   uint32_t buf_end;
} v3d_buf_t;

static v3d_buf_t* v3d_bufs = 0;
static v3d_buf_t* v3d_bufs_end = 0;

static void add_v3d_buf(uint32_t buf_type, uint32_t buf_start, uint32_t buf_end);
static void pop_v3d_buf(void);
static void init_dis_state(dis_state_t* state, uint32_t start_address, uint32_t end_address);
static int increase_dis_area(dis_state_t* state);
static uint32_t virt_to_dis_addr(void* addr, dis_state_t* state);
static int is_cl_end(uint8_t* ins);
static void add_buf_references(void* ins, uint32_t end_address);
static int dis_cl(uint32_t start_address, uint32_t end_address);
static int dis_gl_shader_rec(uint32_t start_address, uint32_t end_address);
static int dis_nv_shader_rec(uint32_t start_address, uint32_t end_address);
static int dis_qpu_prog(uint32_t start_address, uint32_t end_address);

//TODO: if end address is actually inside an instruction this may cause a segmentation error
int do_dis(char* start_addr_str, char* end_addr_str) {
   uint32_t start_addr;
   uint32_t end_addr;

   if(sscanf(start_addr_str, "0x%x", &start_addr) != 1) {
      fprintf(stderr, "Addresses must be of form 0x1234ABCD\n");
      return 1;
   }

   if(sscanf(end_addr_str, "0x%x", &end_addr) != 1) {
      fprintf(stderr, "Addresses must be of form 0x1234ABCD\n");
      return 1;
   }

   add_v3d_buf(BUF_TYPE_CL, start_addr, end_addr);

   printf("Disassembling CL start: %08x end: %08x\n", start_addr, end_addr);
   
   while(v3d_bufs) {
      switch(v3d_bufs->buf_type) {
         case BUF_TYPE_CL:
            if(dis_cl(v3d_bufs->buf_start, v3d_bufs->buf_end)) {
               fprintf(stderr, "Failed to disassemble CL buf start: %08x end: %08x\n", v3d_bufs->buf_start, v3d_bufs->buf_end);
            }
            break;
         case BUF_TYPE_GL_SHADER_REC:
            if(dis_gl_shader_rec(v3d_bufs->buf_start, v3d_bufs->buf_end)) {
               fprintf(stderr, "Failed to disassemble shader rec buf start: %08x end: %08x\n", v3d_bufs->buf_start, v3d_bufs->buf_end);
            }
            break;
         case BUF_TYPE_QPU_PROG:
            if(dis_qpu_prog(v3d_bufs->buf_start, v3d_bufs->buf_end)) {
               fprintf(stderr, "Failed to disassemble QPU buf start: %08x, end: %08x\n", v3d_bufs->buf_start, v3d_bufs->buf_end);
            }
            break;
         case BUF_TYPE_NV_SHADER_REC:
            if(dis_nv_shader_rec(v3d_bufs->buf_start, v3d_bufs->buf_end)) {
               fprintf(stderr, "Failed to disassemble shader rec buf start: %08x end: %08x\n", v3d_bufs->buf_start, v3d_bufs->buf_end);
            }
            break;
         default:
            fprintf(stderr, "Seen invalid buffer type %d whilst disassembling\n", v3d_bufs->buf_type);
      }

      pop_v3d_buf();
   }

   return 0;
}

static void add_v3d_buf(uint32_t buf_type, uint32_t buf_start, uint32_t buf_end) {
   v3d_buf_t* new_buf = malloc(sizeof(v3d_buf_t));

   new_buf->buf_type  = buf_type;
   new_buf->buf_start = buf_start;
   new_buf->buf_end   = buf_end;
   new_buf->next      = 0;


#ifdef CL_DUMP_DEBUG
   printf("Adding disassembly buffer start: %08x, end: %08x, type: %d\n", buf_start, buf_end, buf_type);
#endif

   if(v3d_bufs) {
      assert(v3d_bufs_end);

      v3d_bufs_end->next = new_buf;
      v3d_bufs_end = new_buf;
   } else {
      v3d_bufs = v3d_bufs_end = new_buf;
   }
}

static void pop_v3d_buf() {
   v3d_buf_t* to_pop = v3d_bufs;

   assert(to_pop);

   v3d_bufs = v3d_bufs->next;

   if(to_pop == v3d_bufs_end) {
      assert(v3d_bufs_end->next == 0);

      v3d_bufs_end = 0;
   }

   free(to_pop);
}

static void init_dis_state(dis_state_t* state, uint32_t start_address, uint32_t end_address) {
   memset(state, 0, sizeof(dis_state_t));
   state->current_area_size = 1;
   state->start_address = start_address;
   state->end_address = end_address;
}

static int increase_dis_area(dis_state_t* state) {
   uint32_t cur_ins_offset;

   cur_ins_offset = state->cur_ins - state->cl_start;

   if(state->cl_start) {
      unmap_area(state->cl_start, state->current_area_size);
   }
   
   state->current_area_size *= 2;

#ifdef CL_DUMP_DEBUG
   printf("Expanding diassembly memory area to size %d cur_ins: %p\n", state->current_area_size, state->cur_ins);
#endif

   if(state->current_area_size > MAX_CL_SIZE) {
      fprintf(stderr, "Runaway disassembly memory area\n");
      return 2;
   }

   state->cl_start = map_area(state->start_address, state->current_area_size);
   
   if(!state->cl_start) {
      fprintf(stderr, "Failed to map CL memory\n");
      return 1;
   }

   state->cur_ins = state->cl_start + cur_ins_offset;

   if(state->end_address) {
      state->cl_end = (state->end_address - state->start_address) + state->cl_start;
   } else {
      state->cl_end = 0;
   }

#ifdef CL_DUMP_DEBUG
   printf("New cur_ins: %p\n", state->cur_ins);
#endif

   return 0;
}

static uint32_t virt_to_dis_addr(void* addr, dis_state_t* state) {
   return (addr - state->cl_start) + state->start_address;
}

static int is_cl_end(uint8_t* ins) {
   if(*ins == V3D_HW_INSTR_HALT ||
      *ins == V3D_HW_INSTR_BRANCH ||
      *ins == V3D_HW_INSTR_RETURN) {
      return 1;
   }

   return 0;
}

static void add_buf_references(void* ins, uint32_t end_address) {
   uint8_t* opcode = ins;
   switch(*opcode) {
      case V3D_HW_INSTR_BRANCH_SUB: {
         instr_BRANCH_SUB_t* branch_ins = ins;
         add_v3d_buf(BUF_TYPE_CL, branch_ins->branch_addr, 0);
         break;
      }
      case V3D_HW_INSTR_BRANCH: {
         instr_BRANCH_t* branch_ins = ins;
         //A BRANCH is effectively continuing the CL elsewhere (we cannot RETURN).
         //So inherit the end_address so we know when we've hit the end in the new
         //CL buffer.
         add_v3d_buf(BUF_TYPE_CL, branch_ins->branch_addr, end_address);
         break;
      }
      case V3D_HW_INSTR_GL_SHADER: {
         instr_GL_SHADER_t* glshader_ins = ins;

         uint32_t shader_record_addr = glshader_ins->shader_record_addr << 4;
         uint32_t buf_type;
         uint32_t buf_size;


         buf_size = sizeof(instr_GL_SHADER_RECORD_t) 
            + sizeof(instr_ATTR_ARRAY_RECORD_t) * glshader_ins->num_attr_arrays;
         
         if(glshader_ins->extended_record) {
            buf_type = BUF_TYPE_GL_SHADER_REC_EXT;
            //TODO: workout what to do with buf_size
         } else {
            buf_type = BUF_TYPE_GL_SHADER_REC;
         }

         add_v3d_buf(buf_type, shader_record_addr, shader_record_addr + buf_size);
      }
      case V3D_HW_INSTR_NV_SHADER: {
         instr_NV_SHADER_t* nvshader_ins = ins;

         uint32_t shader_record_addr = nvshader_ins->shader_record_addr;

         add_v3d_buf(BUF_TYPE_NV_SHADER_REC, shader_record_addr, shader_record_addr + sizeof(instr_NV_SHADER_RECORD_t));
      }

   }
}

static int dis_cl(uint32_t start_address, uint32_t end_address) {
   dis_state_t state;

   init_dis_state(&state, start_address, end_address);

   printf("CL buffer addr: %08x\n", start_address);
   printf("------------------------\n");

   increase_dis_area(&state);

   while((!state.cl_end || (state.cur_ins < state.cl_end))) {
      void* next_ins;

      next_ins = calc_next_ins(state.cur_ins);
      if(next_ins == 0) { //Invalid opcode
         next_ins = state.cur_ins + 1; 
      }

      if(next_ins > state.cl_start + state.current_area_size) {
         if(increase_dis_area(&state)) {
            fprintf(stderr, "Failed to expand disassembly memory area\n");
            return 1;
         }

         next_ins = calc_next_ins(state.cur_ins);
         if(next_ins == 0) {
            next_ins = state.cur_ins + 1; //Invalid opcode
         }
      }

      printf("%08x: ", virt_to_dis_addr(state.cur_ins, &state));
      if(disassemble_instr(state.cur_ins, stdout)) {
         printf("INVALID OPCODE (%d)\n", (uint32_t)(*(uint8_t*)state.cur_ins));
      }

      add_buf_references(state.cur_ins, state.end_address);

      if(is_cl_end(state.cur_ins)) {
         unmap_area(state.cl_start, state.current_area_size);
         return 0;
      }

      state.cur_ins = next_ins;

      if(state.cur_ins >= state.cl_start + state.current_area_size) {
         if(increase_dis_area(&state)) {
            fprintf(stderr, "Failed to expand disassembly memory area\n");
            return 1;
         }
      }
   }

   unmap_area(state.cl_start, state.current_area_size);

   printf("\n\n");

   return 0;
}

static int dis_gl_shader_rec(uint32_t start_address, uint32_t end_address) {
   instr_GL_SHADER_RECORD_t* shader_rec;
   instr_ATTR_ARRAY_RECORD_t* cur_attr_array;
   instr_ATTR_ARRAY_RECORD_t* attr_array_end;

   void* shader_rec_mem = map_area(start_address, end_address - start_address);

   printf("GL Shader Record Addr: %08x\n", start_address);
   printf("----------------------------\n");

   if(shader_rec_mem == 0) {
      fprintf(stderr, "Failed to map shader record memory\n");
      return 1;
   }

   shader_rec = shader_rec_mem;
   cur_attr_array = shader_rec_mem + sizeof(instr_GL_SHADER_RECORD_t);

   printf("%08X: ", start_address);
   disassemble_GL_SHADER_RECORD(shader_rec, stdout);

   add_v3d_buf(BUF_TYPE_QPU_PROG, shader_rec->fs_code_addr, 0);
   add_v3d_buf(BUF_TYPE_QPU_PROG, shader_rec->vs_code_addr, 0);
   add_v3d_buf(BUF_TYPE_QPU_PROG, shader_rec->cs_code_addr, 0);

   attr_array_end = (end_address - start_address) + shader_rec_mem;

   while(cur_attr_array < attr_array_end) {
      printf("%08X: ", start_address + ((void*)cur_attr_array - shader_rec_mem));
      disassemble_ATTR_ARRAY_RECORD(cur_attr_array, stdout);

      cur_attr_array++;
   }

   unmap_area(shader_rec_mem, end_address - start_address);

   printf("\n\n");

   return 0;
}

static int dis_nv_shader_rec(uint32_t start_address, uint32_t end_address) {
   instr_NV_SHADER_RECORD_t* shader_rec;

   void* shader_rec_mem = map_area(start_address, end_address - start_address);

   printf("NV Shader Record Addr: %08x\n", start_address);
   printf("----------------------------\n");

   if(shader_rec_mem == 0) {
      fprintf(stderr, "Failed to map shader record memory\n");
      return 1;
   }

   shader_rec = shader_rec_mem;

   printf("%08X: ", start_address);
   disassemble_NV_SHADER_RECORD(shader_rec, stdout);

   add_v3d_buf(BUF_TYPE_QPU_PROG, shader_rec->fs_code_addr, 0);

   unmap_area(shader_rec_mem, end_address - start_address);

   printf("\n\n");

   return 0;
}

#define INITIAL_QPU_BUF_SIZE 4096

static int dis_qpu_prog(uint32_t start_address, uint32_t end_address) {
   void* qpu_prog;
   uint32_t prog_size; //Measured in instructions
   uint32_t mapped_area_size; //Measured in bytes

   printf("QPU Program Addr: %08x\n", start_address);
   printf("--------------------------\n");

   if(end_address == 0) { 
      uint32_t  search_area_size = INITIAL_QPU_BUF_SIZE;
      uint64_t* current_instruction;
      uint32_t  found_end;

      while(1) {
         //Need to search for the program end, this occurs two instructions after we see a program end signal
         qpu_prog = map_area(start_address, search_area_size);
         if(!qpu_prog) {
            fprintf(stderr, "Failed to map QPU program memory with size %d\n", search_area_size);
            return 1;
         }

         current_instruction = qpu_prog;
         prog_size           = 0;
         found_end           = 0;

         while(((void*)current_instruction - qpu_prog) < search_area_size) {
            ++prog_size;
            //Signalling field is bits 63-60 of instruction
            //4'd3 == program end
            if(((*current_instruction) >> 60) == 0x3) {
               prog_size += 2;
               found_end = 1;
               break;
            }

            ++current_instruction;
         }

         if(found_end) {
            mapped_area_size = search_area_size;
            break;
         }

         unmap_area(qpu_prog, search_area_size);
         search_area_size *= 2;
      }
   } else {
      prog_size = (end_address - start_address) / 8;
      mapped_area_size = prog_size * 8;

      qpu_prog = map_area(start_address, mapped_area_size);
   }

   show_qpu_fragment(qpu_prog, prog_size*2);

   unmap_area(qpu_prog, mapped_area_size);

   return 0;
}

