/*
 * This file contains code from SimonJHall, license under a dual BSD/GPL license
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/pagemap.h>

#include <asm/io.h>
#include <asm/atomic.h>
#include <asm/cacheflush.h>
#include <asm/uaccess.h>

#include <mach/dma.h>

#include "vc_support.h"
#include "v3d_ioctl.h"

#define V3D_DEV_NAME "v3d"
#define BIN_MEM_GIVE_SIZE 512 * 1024 * 1024 //Give binner 512 kb chunks when it's out of memory

//SimonJHall code begin
struct V3dRegisterIdent
{
	volatile unsigned int m_ident0;
	volatile unsigned int m_ident1;
	volatile unsigned int m_ident2;
};
static struct V3dRegisterIdent __iomem *g_pV3dIdent = (struct V3dRegisterIdent __iomem *) IO_ADDRESS(0x20c00000);


struct V3dRegisterScratch
{
	volatile unsigned int m_scratch;
};
static struct V3dRegisterScratch __iomem *g_pV3dScratch = (struct V3dRegisterScratch __iomem *) IO_ADDRESS(0x20c00000 + 0x10);


struct V3dRegisterCache
{
	volatile unsigned int m_l2cactl;
	volatile unsigned int m_slcactl;
};
static struct V3dRegisterCache __iomem *g_pV3dCache = (struct V3dRegisterCache __iomem *) IO_ADDRESS(0x20c00000 + 0x20);


struct V3dRegisterIntCtl
{
	volatile unsigned int m_intctl;
	volatile unsigned int m_intena;
	volatile unsigned int m_intdis;
};
static struct V3dRegisterIntCtl __iomem *g_pV3dIntCtl = (struct V3dRegisterIntCtl __iomem *) IO_ADDRESS(0x20c00000 + 0x30);


struct V3dRegisterCle
{
	volatile unsigned int m_ct0cs;
	volatile unsigned int m_ct1cs;
	volatile unsigned int m_ct0ea;
	volatile unsigned int m_ct1ea;
	volatile unsigned int m_ct0ca;
	volatile unsigned int m_ct1ca;
	volatile unsigned int m_ct00ra0;
	volatile unsigned int m_ct01ra0;
	volatile unsigned int m_ct0lc;
	volatile unsigned int m_ct1lc;
	volatile unsigned int m_ct0pc;
	volatile unsigned int m_ct1pc;
	volatile unsigned int m_pcs;
	volatile unsigned int m_bfc;
	volatile unsigned int m_rfc;
};
static volatile struct V3dRegisterCle __iomem *g_pV3dCle = (struct V3dRegisterCle __iomem *) IO_ADDRESS(0x20c00000 + 0x100);


struct V3dRegisterBinning
{
	volatile unsigned int m_bpca;
	volatile unsigned int m_bpcs;
	volatile unsigned int m_bpoa;
	volatile unsigned int m_bpos;
	volatile unsigned int m_bxcf;
};
static struct V3dRegisterBinning __iomem *g_pV3dBinning = (struct V3dRegisterBinning __iomem *) IO_ADDRESS(0x20c00000 + 0x300);


struct V3dRegisterSched
{
	volatile unsigned int m_sqrsv0;
	volatile unsigned int m_sqrsv1;
	volatile unsigned int m_sqcntl;
};
static struct V3dRegisterSched __iomem *g_pV3dSched = (struct V3dRegisterSched __iomem *) IO_ADDRESS(0x20c00000 + 0x410);


struct V3dRegisterUserProgramReq
{
	volatile unsigned int m_srqpc;
	volatile unsigned int m_srqua;
	volatile unsigned int m_srqul;
	volatile unsigned int m_srqcs;
};
static struct V3dRegisterUserProgramReq __iomem *g_pV3dUpr = (struct V3dRegisterUserProgramReq __iomem *) IO_ADDRESS(0x20c00000 + 0x430);


struct V3dRegisterVpm
{
	volatile unsigned int m_vpacntl;
	volatile unsigned int m_vpmbase;
};
static struct V3dRegisterVpm __iomem *g_pV3dVpm = (struct V3dRegisterVpm __iomem *) IO_ADDRESS(0x20c00000 + 0x500);


struct V3dRegisterPcCtl
{
	volatile unsigned int m_pctrc;
	volatile unsigned int m_pctre;
};
static struct V3dRegisterPcCtl __iomem *g_pV3dPcCtl = (struct V3dRegisterPcCtl __iomem *) IO_ADDRESS(0x20c00000 + 0x670);

struct V3dRegisterDebugError
{
	volatile unsigned int m_dbge;
	volatile unsigned int m_fdbgo;
	volatile unsigned int m_fdbgb;
	volatile unsigned int m_fdbgr;
	volatile unsigned int m_fdbgs;
	volatile unsigned int dummy14;
	volatile unsigned int dummy18;
	volatile unsigned int dummy1c;
	volatile unsigned int m_errStat;
};
static struct V3dRegisterDebugError __iomem *g_pV3dDebErr = (struct V3dRegisterDebugError __iomem *) IO_ADDRESS(0x20c00000 + 0xf00);
//SimonJHall code end

static atomic_t v3d_opened = ATOMIC_INIT(0);

static unsigned int v3d_mem = 0;
static unsigned int v3d_mem_bus = 0;
static unsigned int v3d_mem_handle;
static unsigned int v3d_mem_pages;

static void v3d_reset() {
   QpuEnable(false);
   QpuEnable(true);

   g_pV3dIntCtl->m_intdis = 0xF;

   barrier();
}

static int v3d_init() {
   unsigned int ident[3];

   QpuEnable(true);
   
   ident[0] = g_pV3dIdent->m_ident0;
   ident[1] = g_pV3dIdent->m_ident1;
   ident[2] = g_pV3dIdent->m_ident2;

   printk(KERN_DEBUG "Maybe need some dodgy forced wait?");

   if((ident[0] & 0xFFFFFF) != (('D' << 16) | ('3' << 8) | 'V')) {
      printk(KERN_DEBUG "Failed to find V3D ident %X %X %X\n", ident[0], ident[1], ident[2]);
      QpuEnable(false);
      return -1;
   }

   printk(KERN_DEBUG "V3D IDENT Tech Version: %d, Tech Revision: %d\n", ident[0] >> 24, ident[1] & 0xF);
   printk(KERN_DEBUG "VPM: %d HDR: %d NSEM: %d TUPS: %d QUPS: %d NSLC: %d\n", 
          (ident[1] >> 28) & 0xF,
          (ident[1] >> 24) & 0xF,
          (ident[1] >> 16) & 0xFF,
          (ident[1] >> 12) & 0xF,
          (ident[1] >>  8) & 0xF,
          (ident[1] >>  4) & 0xF);
   printk(KERN_DEBUG "TLBDB: %d TLBSZ: %d WRISZ: %d\n\n",
           (ident[2] >>  8) & 0xF,
           (ident[2] >>  4) & 0xF,
            ident[2]        & 0xF);

   return 0;
}

static int v3d_open(struct inode* inode, struct file* file) 
{
   if(atomic_inc_and_test(&v3d_opened) != 0) {
      printk(KERN_DEBUG "Something else has V3D open, open failed\n");
      atomic_dec(&v3d_opened);
      return -EBUSY;
   }

   if(v3d_init()) {
      printk(KERN_DEBUG "V3D init fail, open failed\n");
      atomic_dec(&v3d_opened);
      return -EIO;
   }

   return 0;
}

void v3d_free_mem() 
{
   if(!v3d_mem) {
      //No memory allocated 
      return;
   }

   if(UnlockVcMemory(v3d_mem_handle)) {
      printk(KERN_ERR "failed to unlock V3D memory\n");
   }

   v3d_mem = 0;

   if(ReleaseVcMemory(v3d_mem_handle)) {
      printk(KERN_ERR "Failed to release V3D memory\n");      
   }
}

int v3d_alloc_mem(unsigned int pages)
{
   if(v3d_mem) {
      //Can only have a single block of VC memory allocated at once
      return -1;
   }

   if(AllocateVcMemory(&v3d_mem_handle, pages * 4096, 4096, MEM_FLAG_COHERENT)) {
      printk(KERN_ERR "Could not allocate V3D memory\n");
      return -EINVAL;
   }

   if(LockVcMemory(&v3d_mem_bus, v3d_mem_handle)) {
      printk(KERN_ERR "Could not lock V3D memory\n");
      
      ReleaseVcMemory(v3d_mem_handle);

      return -EINVAL;
   }

   v3d_mem = v3d_mem_bus & 0x3FFFFFFF;
   v3d_mem_pages = pages;

   printk(KERN_DEBUG "Allocated %d bytes of V3D memory bus address: %X phys address: %x\n", pages * 4096, v3d_mem_bus, v3d_mem);

   return 0;
}

static int v3d_release(struct inode* inode, struct file* file)
{
   atomic_dec(&v3d_opened);

   printk(KERN_DEBUG "Closing V3D, freeing V3D memory and disabling V3D block\n");
   v3d_free_mem();

   QpuEnable(false);

   return 0;
}

static int wait_v3d_idle() {
   int timeout = 10000;
   while(timeout) {
      if(!(g_pV3dCle->m_pcs & 0xF))
         break;

      timeout--;
      schedule();
   }

   if(timeout <= 0) {
      printk(KERN_ERR "Timeout during v3d wait, resetting v3d\n");
      v3d_reset();

      return -1;
   }

	__cpuc_flush_user_all();
	__cpuc_flush_kern_all();

   return 0;
}

static int run_v3d_job(v3d_job_t* job) {
   unsigned int current_bin_flushes;
   unsigned int current_rdr_completes;
   unsigned int timeout;
   unsigned int bin_done;
   unsigned int rdr_done;

   wait_v3d_idle();

   //Clear l1 and l2 caches.
   g_pV3dCache->m_slcactl = 0x0f0f0f0f;
   g_pV3dCache->m_l2cactl = 1 << 2;

   barrier();

	__cpuc_flush_user_all();
	__cpuc_flush_kern_all();

   //Kick off binning thread
   g_pV3dCle->m_ct0cs = 1 << 5;
   barrier();
   g_pV3dCle->m_ct0ca = job->bin_start;
   barrier();
   g_pV3dCle->m_ct0ea = job->bin_end;
   barrier();

   timeout  = 1000000;
   bin_done = 0;
   rdr_done = 0;

   current_bin_flushes   = g_pV3dCle->m_bfc;
   current_rdr_completes = g_pV3dCle->m_rfc;

   printk(KERN_ERR "Running V3D job Bin %x %x Rdr %x %x Overspill %d bytes from %X\n",
           job->bin_start, job->bin_end,
           job->rdr_start, job->rdr_end,
           job->bin_overspill, job->bin_overspill_size);

   while(timeout) {
      if(g_pV3dCle->m_pcs & 0x100) {
         unsigned int mem_to_give = BIN_MEM_GIVE_SIZE;
         //Out of binning memory
         if(job->bin_overspill_size == 0) {
            printk(KERN_ERR "Ran out of binning overspill memory\n");
            v3d_reset();

            return -1;
         }

         if(mem_to_give > job->bin_overspill_size) {
            mem_to_give = job->bin_overspill_size;
         }
         printk(KERN_ERR "Out of binning overspill memory, giving it %d bytes more\n", mem_to_give);

         g_pV3dBinning->m_bpoa = job->bin_overspill;
         barrier();
         g_pV3dBinning->m_bpos = mem_to_give;
         barrier();

         job->bin_overspill_size -= mem_to_give;
         job->bin_overspill      += mem_to_give;

         unsigned int current_addr = g_pV3dCle->m_ct0ca;
         
	     while (timeout--)
	     {
	     	if (g_pV3dCle->m_ct0ca != current_addr)
	     		break;
            schedule();
	     }
	     timeout = 1000000;
      }

      if(!bin_done) {
         int bin_sts = g_pV3dCle->m_ct0cs;

         if(bin_sts & 0x8) {
            printk(KERN_ERR "Binning thread error sts: %x ca: %x\n", bin_sts, g_pV3dCle->m_ct0ca);

            v3d_reset();
            return -2;
         }

         if(!(bin_sts & 0x20)) {
            //Bin thread has stopped at end
            if(current_bin_flushes != g_pV3dCle->m_bfc) {
               //Change in BFC so we're done
               printk(KERN_DEBUG "Binning thread complete\n");
               bin_done = 1;
               //Kick off render thread
               g_pV3dCle->m_ct1cs = 1 << 5;
               barrier();
               g_pV3dCle->m_ct1ca = job->rdr_start;
               barrier();
               g_pV3dCle->m_ct1ea = job->rdr_end;

               timeout = 1000000;
            }
         }
      }

      if(!rdr_done) {
         int rdr_sts = g_pV3dCle->m_ct0cs;

         if(rdr_sts & 0x8) {
            printk(KERN_ERR "Rendering thread error sts: %x ca: %x\n", rdr_sts, g_pV3dCle->m_ct0ca);

            v3d_reset();
            return -3;
         }

         if(!(rdr_sts & 0x20)) {
            //rdr thread has stopped at end
            if(current_rdr_completes != g_pV3dCle->m_rfc) {
               //Change in RFC so we're done
               printk(KERN_DEBUG "Rendering thread complete\n");
               rdr_done = 1;

               return 0;
            }
         }
      }

      timeout--;
      schedule();
   }

   printk(KERN_DEBUG "V3D job timeout out bin CA: %x CS: %x rdr CA: %x CS: %x PCS: %x\n", g_pV3dCle->m_ct0ca, g_pV3dCle->m_ct0cs,
         g_pV3dCle->m_ct1ca, g_pV3dCle->m_ct1cs, g_pV3dCle->m_pcs);

   return -4;
}

static long v3d_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
   switch(cmd)
   {
      case V3D_IOCTL_SUBMIT_JOB: {
         v3d_job_t __user *user_job = (v3d_job_t __user *)arg;
         v3d_job_t job;

         if(copy_from_user(&job, user_job, sizeof(v3d_job_t))) {
            printk(KERN_ERR "Could not copy V3D job from user\n");
            return -EFAULT;
         }

         return run_v3d_job(&job);
      }
      case V3D_IOCTL_WAIT:
         return wait_v3d_idle();
      case V3D_IOCTL_MEM_ALLOC:
         if(v3d_alloc_mem(arg)) {
            return -EINVAL;
         }

         return v3d_mem_bus;
      case V3D_IOCTL_MEM_FREE:
         v3d_free_mem();

         return 0;
   }

   return -EINVAL;
}

static int v3d_mmap(struct file* file, struct vm_area_struct *vma)
{
   unsigned long size;

   size = vma->vm_end - vma->vm_start;

   printk(KERN_DEBUG "Mapping memory of size %d bytes\n", size);
   if((size + (vma->vm_pgoff << PAGE_SHIFT)) > (v3d_mem_pages << PAGE_SHIFT)) {
      printk(KERN_ERR "Tried to mmap beyond V3D memory\n");
      return -EINVAL;
   }

   vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

   if(remap_pfn_range(vma, vma->vm_start, (v3d_mem >> PAGE_SHIFT) + vma->vm_pgoff, size, vma->vm_page_prot)) {
      printk(KERN_ERR "Remap failed on V3D mmap\n");
      return -EAGAIN;
   }

   return 0;
}

static struct file_operations v3d_fops = {
   .owner          = THIS_MODULE,
   .unlocked_ioctl = v3d_ioctl,
   .mmap           = v3d_mmap,
   .open           = v3d_open,
   .release        = v3d_release
};

static struct miscdevice v3d_miscdev = {
   .minor = MISC_DYNAMIC_MINOR,
   .name  = V3D_DEV_NAME,
   .fops  = &v3d_fops
};

static int __init v3d_module_init(void) 
{
   int misc_reg_err;

   misc_reg_err = misc_register(&v3d_miscdev);
   if(misc_reg_err) {
      printk(KERN_ERR "Unable to register V3D device: %d\n", misc_reg_err);
      return misc_reg_err;
   }

   return 0;
}

static void __exit v3d_module_exit(void)
{
   misc_deregister(&v3d_miscdev);
}

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Greg Chadwick");
module_init(v3d_module_init);
module_exit(v3d_module_exit);

