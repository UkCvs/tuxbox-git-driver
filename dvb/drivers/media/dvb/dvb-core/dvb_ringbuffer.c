/*
 *
 * dvb_ringbuffer.c: ring buffer implementation for the dvb driver
 *
 * Copyright (C) 2003 Oliver Endriss 
 * 
 * based on code originally found in av7110.c:
 * Copyright (C) 1999-2002 Ralph  Metzler 
 *                       & Marcus Metzler for convergence integrated media GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 * 
 *
 * the project's page is at http://www.linuxtv.org/dvb/
 */



#define __KERNEL_SYSCALLS__
#include <asm/uaccess.h>
#include <asm/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/string.h>

#include "dvb_ringbuffer.h"



void dvb_ringbuffer_init(dvb_ringbuffer_t *rbuf, void *data, size_t len)
{
        rbuf->pread=rbuf->pwrite=0;
        rbuf->data=data;
        rbuf->size=len;

        init_waitqueue_head(&rbuf->queue);

        spin_lock_init(&(rbuf->lock));
}



int dvb_ringbuffer_empty(dvb_ringbuffer_t *rbuf)
{
        return (rbuf->pread==rbuf->pwrite);
}



ssize_t dvb_ringbuffer_free(dvb_ringbuffer_t *rbuf)
{
        ssize_t free;
  
        free = rbuf->pread - rbuf->pwrite;
        if (free <= 0)
                free += rbuf->size;
        return free-1;
}



ssize_t dvb_ringbuffer_avail(dvb_ringbuffer_t *rbuf)
{
        ssize_t avail;
  
        avail = rbuf->pwrite - rbuf->pread;
        if (avail < 0)
                avail += rbuf->size;
        return avail;
}



void dvb_ringbuffer_flush(dvb_ringbuffer_t *rbuf)
{
        rbuf->pread = rbuf->pwrite;
}



void dvb_ringbuffer_flush_spinlock_wakeup(dvb_ringbuffer_t *rbuf)
{
        unsigned long flags;

        spin_lock_irqsave(&rbuf->lock, flags);
        dvb_ringbuffer_flush(rbuf);
        spin_unlock_irqrestore(&rbuf->lock, flags);

        wake_up(&rbuf->queue);
}



ssize_t dvb_ringbuffer_read(dvb_ringbuffer_t *rbuf, u8 *buf, size_t len, int usermem)
{
        size_t todo = len;
        size_t split;

        split = (rbuf->pread + len > rbuf->size) ? rbuf->size - rbuf->pread : 0;
        if (split > 0) {
                if (!usermem)
                        memcpy(buf, rbuf->data+rbuf->pread, split);
                else
                        if (copy_to_user(buf, rbuf->data+rbuf->pread, split))
                                return -EFAULT;
                buf += split;
                todo -= split;
                rbuf->pread = 0;
        }
        if (!usermem)
                memcpy(buf, rbuf->data+rbuf->pread, todo);
        else
                if (copy_to_user(buf, rbuf->data+rbuf->pread, todo))
                        return -EFAULT;

        rbuf->pread = (rbuf->pread + len) % rbuf->size;

        return len;
}



ssize_t dvb_ringbuffer_write(dvb_ringbuffer_t *rbuf, const u8 *buf,
                             size_t len, int usermem)
{
        size_t todo = len;
        size_t split;
    
        split = (rbuf->pwrite + len > rbuf->size) ? rbuf->size - rbuf->pwrite : 0;

        if (split > 0) {
                if (!usermem) 
                        memcpy(rbuf->data+rbuf->pwrite, buf, split);
                else
                        if (copy_from_user(rbuf->data+rbuf->pwrite, 
                                           buf, split))
                                return -EFAULT;
                buf += split;
                todo -= split;
                rbuf->pwrite = 0;
        }
        if (!usermem) 
                memcpy(rbuf->data+rbuf->pwrite, buf, todo);
        else
                if (copy_from_user(rbuf->data+rbuf->pwrite, buf, todo)) 
                        return -EFAULT;

        rbuf->pwrite = (rbuf->pwrite + len) % rbuf->size;

	return len;
}


EXPORT_SYMBOL_GPL(dvb_ringbuffer_init);
EXPORT_SYMBOL_GPL(dvb_ringbuffer_empty);
EXPORT_SYMBOL_GPL(dvb_ringbuffer_free);
EXPORT_SYMBOL_GPL(dvb_ringbuffer_avail);
EXPORT_SYMBOL_GPL(dvb_ringbuffer_flush);
EXPORT_SYMBOL_GPL(dvb_ringbuffer_flush_spinlock_wakeup);
EXPORT_SYMBOL_GPL(dvb_ringbuffer_read);
EXPORT_SYMBOL_GPL(dvb_ringbuffer_write);
