#ifndef __CRAP_H
#define __CRAP_H

/**
 *  compatibility crap for old kernels. No guarantee for a working driver
 *  even when everything compiles.
 */


#include <linux/module.h>
#include <linux/list.h>
#include <linux/videodev.h>


#ifndef MODULE_LICENSE
#define MODULE_LICENSE(x)
#endif


#ifndef list_for_each_safe
#define list_for_each_safe(pos, n, head) \
        for (pos = (head)->next, n = pos->next; pos != (head); \
                pos = n, n = pos->next)
#endif


#ifndef __devexit_p
#if defined(MODULE)
#define __devexit_p(x) x
#else
#define __devexit_p(x) NULL
#endif
#endif


#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || !CONFIG_VIDEO_DEV
#define video_usercopy generic_usercopy

extern int generic_usercopy(struct inode *inode, struct file *file,
	                    unsigned int cmd, unsigned long arg,
			    int (*func)(struct inode *inode, struct file *file,
			    unsigned int cmd, void *arg));
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)

#define video_devdata(dev) (dev->priv)

static inline
void cond_resched (void)
{
	if (current->need_resched)
		schedule();
}

extern struct page * vmalloc_to_page(void *addr);

#define remap_page_range(vma,from,to,size,prot) \
	remap_page_range(from,to,size,prot)

#endif


#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
extern u32 crc32_le (u32 crc, unsigned char const *p, size_t len);
#else
#include <linux/crc32.h>
#endif

#endif

