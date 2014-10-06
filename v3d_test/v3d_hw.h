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

#ifndef __V3D_HW_H__
#define __V3D_HW_H__

#include <linux/ioctl.h>
#include <stdint.h>

struct GPU_PTR {
    unsigned vc;
    union { struct GPU_FFT_COMPLEX *cptr;
            void                   *vptr;
            char                   *bptr;
            float                  *fptr;
            unsigned               *uptr; } arm;
};

struct GPU_BASE {
    uint32_t mem_size;
    int      v3d_fd;
    struct GPU_PTR mem_base;

    volatile uint32_t *peri;
};

int v3d_init (
    char* v3d_dev,
    uint32_t mem_size,
    struct GPU_BASE* base);
void v3d_shutdown(struct GPU_BASE *base);
void v3d_reset(struct GPU_BASE* base);
int v3d_run_job(struct GPU_BASE* base,
        uint32_t bin_cl, uint32_t bin_end,
        uint32_t rdr_cl, uint32_t rdr_end,
        uint32_t bin_overspill, uint32_t bin_overspill_size);

#endif

