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

#include "v3d_hw.h"

#define DEBUG

#define PAGE_SIZE (4*1024)

#define PERI_BASE 0x20000000
#define PERI_SIZE 0x02000000

// V3D spec: http://www.broadcom.com/docs/support/videocore/VideoCoreIV-AG100-R.pdf
#define V3D_IDENT0  (0xC00000>>2)
#define V3D_IDENT1  (0xC00004>>2)
#define V3D_IDENT2  (0xC00008>>2)

#define V3D_L2CACTL (0xC00020>>2)
#define V3D_SLCACTL (0xC00024>>2)

#define V3D_INTCTL  (0xC00030>>2)
#define V3D_INTENA  (0xC00034>>2)
#define V3D_INTDIS  (0xC00038>>2)

#define V3D_CT0CS   (0xC00100>>2)
#define V3D_CT1CS   (0xC00104>>2)
#define V3D_CT0EA   (0xC00108>>2)
#define V3D_CT1EA   (0xC0010C>>2)
#define V3D_CT0CA   (0xC00110>>2)
#define V3D_CT1CA   (0xC00114>>2)
#define V3D_CT00RA0 (0xC00118>>2)
#define V3D_CT01RA0 (0xC0011C>>2)
#define V3D_CT0LC   (0xC00120>>2)
#define V3D_CT1LC   (0xC00124>>2)
#define V3D_CT0PC   (0xC00128>>2)
#define V3D_CT1PC   (0xC0012C>>2)

#define V3D_PCS     (0xC00130>>2)
#define V3D_BFC     (0xC00134>>2)
#define V3D_RFC     (0xC00138>>2)

#define V3D_BPCA    (0xC00300>>2)
#define V3D_BPCS    (0xC00304>>2)
#define V3D_BPOA    (0xC00308>>2)
#define V3D_BPOS    (0xC0030C>>2)


#define V3D_DBCFG   (0xC00e00>>2)
#define V3D_DBQITE  (0xC00e2c>>2)
#define V3D_DBQITC  (0xC00e30>>2)

#define GPU_MEM_FLG 0xC // cached=0xC; direct=0x4
#define GPU_MEM_MAP 0x0 // cached=0x0; direct=0x20000000

int v3d_init (
    int mb,
    uint32_t mem_size,
    struct GPU_BASE* base) {

    volatile uint32_t *peri;
    uint32_t handle;

    uint32_t ident[3];

    if (qpu_enable(mb, 1)) {
        fprintf(stderr, "Failed to enable V3D\n");   
        return -1;
    }

    // Shared memory
    handle = mem_alloc(mb, mem_size, 4096, GPU_MEM_FLG);
    if (!handle) {
        fprintf(stderr, "Failed to allocate VC mem\n");
        qpu_enable(mb, 0);
        return -3;
    }

    peri = (volatile unsigned *) mapmem(PERI_BASE, PERI_SIZE);
    if (!peri) {
        fprintf(stderr, "Failed to map peripheral space\n");
        mem_free(mb, handle);
        qpu_enable(mb, 0);
        return -4;
    }

    base->mem_base.vc = mem_lock(mb, handle);
    base->mem_base.arm.vptr = mapmem(base->mem_base.vc+GPU_MEM_MAP, mem_size);

#ifdef DEBUG
    printf("Allocated V3D base memory at ARM: %p, VC: %x\n", base->mem_base.arm.vptr, base->mem_base.vc);
#endif
    
    base->peri   = peri;
    base->mb     = mb;
    base->mem_handle = handle;
    base->mem_size   = mem_size;

    ident[0] = peri[V3D_IDENT0];
    ident[1] = peri[V3D_IDENT1];
    ident[2] = peri[V3D_IDENT2];

    printf("Testing: %x %x\n", (('D' << 16) | ('3' << 8) | 'V'), (ident[0] & 0xFFFFFF));

    if((ident[0] & 0xFFFFFF) != (('D' << 16) | ('3' << 8) | 'V')) {
        printf("Failed to find V3D\n");
        mem_free(mb, handle);
        qpu_enable(mb, 0);
        return -2;
    }

    printf("V3D IDENT Tech Version: %d, Tech Revision: %d\n", ident[0] >> 24, ident[1] & 0xF);
    printf("IDENT %x %x\n", ident[1], ident[2]);
    printf("VPM: %d HDR: %d NSEM: %d TUPS: %d QUPS: %d NSLC: %d\n", 
            (ident[1] >> 28) & 0xF,
            (ident[1] >> 24) & 0xF,
            (ident[1] >> 16) & 0xFF,
            (ident[1] >> 12) & 0xF,
            (ident[1] >>  8) & 0xF,
            (ident[1] >>  4) & 0xF);
    printf("TLBDB: %d TLBSZ: %d WRISZ: %d\n\n",
            (ident[2] >>  8) & 0xF,
            (ident[2] >>  4) & 0xF,
             ident[2]        & 0xF);

    v3d_reset(base);

    return 0;
}

void v3d_reset(struct GPU_BASE* base) {
    base->peri[V3D_CT0CS] = 1 << 15;
    base->peri[V3D_CT1CS] = 1 << 15;
    base->peri[V3D_SLCACTL] = 0x0F0F0F0F;
    base->peri[V3D_L2CACTL] = 1 << 2;
    base->peri[V3D_INTDIS] = 0xF;
}

int v3d_run_job(struct GPU_BASE* base,
        uint32_t bin_cl, uint32_t bin_end,
        uint32_t rdr_cl, uint32_t rdr_end,
        uint32_t bin_overspill, uint32_t bin_overspill_size) {

    //Wait for V3D idle
    while(base->peri[V3D_PCS] & 0xF);

    uint32_t current_bin_flushes = base->peri[V3D_BFC] & 0xFF;
    uint32_t current_rdr_completes = base->peri[V3D_RFC] & 0xFF;
    uint32_t bin_done = 0;
    uint32_t rdr_done = 0;

    base->peri[V3D_L2CACTL] = 1 << 2;
    base->peri[V3D_CT0CS] = 1 << 5;
    base->peri[V3D_CT0CA] = bin_cl;
    base->peri[V3D_CT0EA] = bin_end;

    uint32_t count = 1000;

    while(count) {
        if(base->peri[V3D_PCS] & 0x100) {
            printf("Bin out of memory, giving it some more\n");
            //Bin out of memory;
            if(bin_overspill_size == 0) {
                printf("No more overspill memory, job failed, resetting\n");
                v3d_reset(base);

                return -1;
            }

            base->peri[V3D_BPOA] = bin_overspill;
            base->peri[V3D_BPOS] = bin_overspill_size;

            bin_overspill_size = 0;
        }

        if(!bin_done) {
            uint32_t bin_sts = base->peri[V3D_CT0CS];

            if(bin_sts & 0x8) {
                printf("Binning thread error! sts: %x, ca: %x\n", bin_sts, base->peri[V3D_CT0CA]);
                v3d_reset(base);

                return -2;
            }

            if(!(bin_sts & 0x20)) {
                if(current_bin_flushes != (base->peri[V3D_BFC] & 0xFF)) {
                    printf("Bin complete!\nStarting render\n");
                    bin_done = 1;

                    base->peri[V3D_CT1CS] = 1 << 5;
                    base->peri[V3D_CT1CA] = rdr_cl;
                    base->peri[V3D_CT1EA] = rdr_end;
                }
            }
        }

        if(!rdr_done) {
            uint32_t rdr_sts = base->peri[V3D_CT1CS];

            if(rdr_sts & 0x8) {
                printf("Rendering thread error! sts: %x, ca: %x\n", rdr_sts, base->peri[V3D_CT1CA]);
                v3d_reset(base);

                return -3;
            }

            if(!(rdr_sts & 0x20)) {
                if(current_rdr_completes != (base->peri[V3D_RFC] & 0xFF)) {
                    printf("Render complete!\n");
                    rdr_done = 1;
                }
            }
        }

        if(rdr_done & bin_done) {
            printf("Job complete\n");
            return 0;
        }

        //printf("Not done yet bin CA: %x CS: %x rdr CA: %x CS: %x, PCS: %x\n", base->peri[V3D_CT0CA], base->peri[V3D_CT0CS], base->peri[V3D_CT1CA], base->peri[V3D_CT1CS], base->peri[V3D_PCS]);

        usleep(10000);
        count--;
    }

    printf("Timed out bin CA: %x CS: %x rdr CA: %x CS: %x PCS: %x\n", base->peri[V3D_CT0CA], base->peri[V3D_CT0CS], base->peri[V3D_CT1CA], base->peri[V3D_CT1CS], base->peri[V3D_PCS]);

    v3d_reset(base);

    return -4;
}

void v3d_shutdown(struct GPU_BASE *base) {
    int mb = base->mb;
    uint32_t handle = base->mem_handle, size = base->mem_size;
    unmapmem((void*)base->peri, PERI_SIZE);
    unmapmem(base->mem_base.arm.vptr, size);
    mem_unlock(mb, handle);
    mem_free(mb, handle);
    qpu_enable(mb, 0);
}

unsigned gpu_ptr_inc (
    struct GPU_PTR *ptr,
    int bytes) {

    unsigned vc = ptr->vc;
    ptr->vc += bytes;
    ptr->arm.bptr += bytes;
    return vc;
}


void *mapmem(unsigned base, unsigned size)
{
   int mem_fd;
   unsigned offset = base % PAGE_SIZE;
   base = base - offset;
   /* open /dev/mem */
   if ((mem_fd = open("/dev/vc-mem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem\nThis program should be run as root. Try prefixing command with: sudo\n");
      exit (-1);
   }
   void *mem = mmap(
      0,
      size,
      PROT_READ|PROT_WRITE,
      MAP_SHARED/*|MAP_FIXED*/,
      mem_fd,
      base);
#ifdef DEBUG
   printf("base=0x%x, mem=%p\n", base, mem);
#endif
   if (mem == MAP_FAILED) {
      printf("mmap error %d\n", (int)mem);
      exit (-1);
   }
   close(mem_fd);
   return (char *)mem + offset;
}

void unmapmem(void *addr, unsigned size)
{
   int s = munmap(addr, size);
   if (s != 0) {
      printf("munmap error %d\n", s);
      exit (-1);
   }
}

/*
 * use ioctl to send mbox property message
 */

static int mbox_property(int file_desc, void *buf)
{

#ifdef DEBUG
   printf("mbox_property sending:\n");
   unsigned *p = buf; int i; unsigned size = *(unsigned *)buf;
   for (i=0; i<size/4; i++)
      printf("%04x: 0x%08x\n", i*sizeof *p, p[i]);
#endif
   int ret_val = ioctl(file_desc, IOCTL_MBOX_PROPERTY, buf);

   if (ret_val < 0) {
      printf("ioctl_set_msg failed:%d\n", ret_val);
   }

#ifdef DEBUG
   printf("mbox_property result:\n");
   for (i=0; i<size/4; i++)
      printf("%04x: 0x%08x\n", i*sizeof *p, p[i]);
#endif
   return ret_val;
}

unsigned mem_alloc(int file_desc, unsigned size, unsigned align, unsigned flags)
{
   int i=0;
   unsigned p[32];
   p[i++] = 0; // size
   p[i++] = 0x00000000; // process request

   p[i++] = 0x3000c; // (the tag id)
   p[i++] = 12; // (size of the buffer)
   p[i++] = 12; // (size of the data)
   p[i++] = size; // (num bytes? or pages?)
   p[i++] = align; // (alignment)
   p[i++] = flags; // (MEM_FLAG_L1_NONALLOCATING)

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof *p; // actual size

   mbox_property(file_desc, p);
   return p[5];
}

unsigned mem_free(int file_desc, unsigned handle)
{
   int i=0;
   unsigned p[32];
   p[i++] = 0; // size
   p[i++] = 0x00000000; // process request

   p[i++] = 0x3000f; // (the tag id)
   p[i++] = 4; // (size of the buffer)
   p[i++] = 4; // (size of the data)
   p[i++] = handle;

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof *p; // actual size

   mbox_property(file_desc, p);
   return p[5];
}

unsigned mem_lock(int file_desc, unsigned handle)
{
   int i=0;
   unsigned p[32];
   p[i++] = 0; // size
   p[i++] = 0x00000000; // process request

   p[i++] = 0x3000d; // (the tag id)
   p[i++] = 4; // (size of the buffer)
   p[i++] = 4; // (size of the data)
   p[i++] = handle;

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof *p; // actual size

   mbox_property(file_desc, p);
   return p[5];
}

unsigned mem_unlock(int file_desc, unsigned handle)
{
   int i=0;
   unsigned p[32];
   p[i++] = 0; // size
   p[i++] = 0x00000000; // process request

   p[i++] = 0x3000e; // (the tag id)
   p[i++] = 4; // (size of the buffer)
   p[i++] = 4; // (size of the data)
   p[i++] = handle;

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof *p; // actual size

   mbox_property(file_desc, p);
   return p[5];
}

unsigned qpu_enable(int file_desc, unsigned enable)
{
   int i=0;
   unsigned p[32];

   p[i++] = 0; // size
   p[i++] = 0x00000000; // process request

   p[i++] = 0x30012; // (the tag id)
   p[i++] = 4; // (size of the buffer)
   p[i++] = 4; // (size of the data)
   p[i++] = enable;

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof *p; // actual size

   mbox_property(file_desc, p);
   return p[5];
}

int mbox_open() {
   int file_desc;

   // open a char device file used for communicating with kernel mbox driver
   file_desc = open(DEVICE_FILE_NAME, 0);
   if (file_desc < 0) {
      printf("Can't open device file: %s\n", DEVICE_FILE_NAME);
      printf("Try creating a device file with: sudo mknod %s c %d 0\n", DEVICE_FILE_NAME, MAJOR_NUM);
      exit(-1);
   }
   return file_desc;
}

void mbox_close(int file_desc) {
  close(file_desc);
}

