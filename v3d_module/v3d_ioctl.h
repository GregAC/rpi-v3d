#ifndef __V3D_IOCTL_H__
#define __V3D_IOCTL_H__

#define V3D_IOCTL_TYPE 'v'

#define V3D_IOCTL_SUBMIT_JOB_NR 0
#define V3D_IOCTL_WAIT_NR       1
#define V3D_IOCTL_MEM_ALLOC_NR  2
#define V3D_IOCTL_MEM_FREE_NR   3

typedef struct 
{
   unsigned int bin_start;
   unsigned int bin_end;
   unsigned int rdr_start;
   unsigned int rdr_end;

   unsigned int bin_overspill;
   unsigned int bin_overspill_size;
} v3d_job_t;

#define V3D_IOCTL_SUBMIT_JOB _IOW(V3D_IOCTL_TYPE, V3D_IOCTL_SUBMIT_JOB_NR, v3d_job_t)
#define V3D_IOCTL_WAIT       _IO(V3D_IOCTL_TYPE, V3D_IOCTL_WAIT_NR)
#define V3D_IOCTL_MEM_ALLOC  _IOW(V3D_IOCTL_TYPE, V3D_IOCTL_MEM_ALLOC_NR, unsigned int)
#define V3D_IOCTL_MEM_FREE   _IO(V3D_IOCTL_TYPE, V3D_IOCTL_MEM_FREE_NR)

#endif

