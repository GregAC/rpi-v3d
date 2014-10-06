/*
This source was taken from hello_fft by Andrew Holme.
It has had some minor modifications by Greg Chadwick.

The original copyright notice is reproduce below:

BCM2835 "GPU_FFT" release 2.0 BETA
Copyright (c) 2014, Andrew Holme.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "v3d_hw.h"
#include "../v3d_module/v3d_ioctl.h"

#define DEBUG

#define PAGE_SIZE (4*1024)

static void unmapmem(void *addr, unsigned size);
static void *mapmem(int v3d_fd, unsigned base, unsigned size);
static void v3d_free_memory(int v3d_fd);

int v3d_init (
    char* v3d_dev,
    uint32_t mem_size,
    struct GPU_BASE* base) {
    
    int v3d_fd = open(v3d_dev, O_RDWR);
    if(v3d_fd < 0) {
        fprintf(stderr, "Failed to open V3D device %s: %s\n", v3d_dev, strerror(errno));
        return -1;
    }

    base->v3d_fd = v3d_fd;

    uint32_t v3d_mem_bus;

    v3d_mem_bus = ioctl(v3d_fd, V3D_IOCTL_MEM_ALLOC, mem_size);
    if(v3d_mem_bus == -1) {
        close(v3d_fd);
        fprintf(stderr, "Failed to allocate V3D memory\n");
        return -2;
    }

    base->mem_base.vc       = v3d_mem_bus;
    base->mem_base.arm.vptr = mapmem(v3d_fd, 0, mem_size * PAGE_SIZE);

    if(base->mem_base.arm.vptr == 0) {
        fprintf(stderr, "Failed to map V3D memory\n");
        v3d_free_memory(v3d_fd);
        return -3;
    }

    base->mem_size = mem_size;

#ifdef DEBUG
    printf("Allocated V3D base memory at ARM: %p, VC: %x\n", base->mem_base.arm.vptr, base->mem_base.vc);
#endif

    return 0;
}

int v3d_run_job(struct GPU_BASE* base,
        uint32_t bin_cl, uint32_t bin_end,
        uint32_t rdr_cl, uint32_t rdr_end,
        uint32_t bin_overspill, uint32_t bin_overspill_size) {

    v3d_job_t job;

    job.bin_start = bin_cl;
    job.bin_end   = bin_end;
    job.rdr_start = rdr_cl;
    job.rdr_end   = rdr_end;

    job.bin_overspill = bin_overspill;
    job.bin_overspill_size = bin_overspill_size;

    printf("Running job Bin %x %x Rdr %x %x Overspill %d bytes at %x\n",
            bin_cl, bin_end,
            rdr_cl, rdr_end,
            bin_overspill, bin_overspill_size);

    int v3d_ioctl_res;

    v3d_ioctl_res = ioctl(base->v3d_fd, V3D_IOCTL_SUBMIT_JOB, &job);
    if(v3d_ioctl_res != 0) {
        fprintf(stderr, "V3D submit job failed with %d\n", v3d_ioctl_res);
        return -1;
    }

    printf("Job submitted, waiting for finish\n");

    v3d_ioctl_res = ioctl(base->v3d_fd, V3D_IOCTL_WAIT);
    if(v3d_ioctl_res != 0) {
        fprintf(stderr, "V3D wait job failed with %d\n", v3d_ioctl_res);
        return -2;
    }
}

void v3d_free_memory(int v3d_fd) {
    if(ioctl(v3d_fd, V3D_IOCTL_MEM_FREE)) {
        fprintf(stderr, "V3D Mem free failed\n");
    }
}

void v3d_shutdown(struct GPU_BASE *base) {
    uint32_t size = base->mem_size;

    unmapmem(base->mem_base.arm.vptr, size * 4096);
    
    v3d_free_memory(base->v3d_fd);

    close(base->v3d_fd);
}

unsigned gpu_ptr_inc (
    struct GPU_PTR *ptr,
    int bytes) {

    unsigned vc = ptr->vc;
    ptr->vc += bytes;
    ptr->arm.bptr += bytes;
    return vc;
}


static void *mapmem(int v3d_fd, unsigned base, unsigned size)
{
   unsigned offset = base % PAGE_SIZE;
   base = base - offset;

   void *mem = mmap(
      0,
      size,
      PROT_READ|PROT_WRITE,
      MAP_SHARED,
      v3d_fd,
      base);
#ifdef DEBUG
   printf("base=0x%x, mem=%p\n", base, mem);
#endif
   if (mem == MAP_FAILED) {
      printf("mmap error %d %s\n", (int)mem, strerror(errno));
      return 0;
   }

   return (char *)mem + offset;
}

static void unmapmem(void *addr, unsigned size)
{
   int s = munmap(addr, size);
   if (s != 0) {
      printf("munmap error %d\n", s);
      exit (-1);
   }
}

