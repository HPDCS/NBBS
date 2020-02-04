/**		      Copyright (C) 2014-2015 HPDCS Group
*		       http://www.dis.uniroma1.it/~hpdcs
* 
* This is free software; 
* You can redistribute it and/or modify this file under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
* 
* This file is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License along with
* this file; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
* 
* @file timestretch.c 
* @brief This is the main source for the Linux Kernel Module which implements
*	 the timestrech module for modifying the quantum assigned to a process.
* @author Francesco Quaglia
*
* @date March 11, 2015
*/
#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/kprobes.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/apic.h>
#include <asm/pgalloc.h>
#include <asm/uaccess.h>

// This gives access to read_cr0() and write_cr0()
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,3,0)
    #include <asm/switch_to.h>
#else
    #include <asm/system.h>
#endif
#ifndef X86_CR0_WP
#define X86_CR0_WP 0x00010000
#endif


#define DEBUG if(0)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Francesco Quaglia <quaglia@dis.uniroma1.it>");
MODULE_DESCRIPTION("nesting of a new system call on the first entry originall reserved for sys_ni_syscall");
MODULE_DESCRIPTION("the new system call logs a message into a kernel buffer");

#define MODNAME "bd_api"

int restore[2] = {[0 ... 1] -1};
unsigned long _force_order_;

/* please take the two values below from the system map */
unsigned long _syscall_table = SYSCALL_TABLE;
unsigned long _ni_syscall = NI_SYSCALL; 


#define AUDIT if (0)
#define MAX_BD_ORDER 5

int valid = 0;
int last_type = -1;

static inline void _write_cr0(unsigned long val)
{
	asm volatile("mov %0, %%cr0" : : "r" (val), "m" (_force_order_));
}


asmlinkage long sys_allocate(int order, unsigned long * address){
	
    unsigned long temp;

	AUDIT{
		printk("%s: sys_allocate has been called with param  %d \n",MODNAME,(int)order);
	}

	if(order > MAX_BD_ORDER) goto bad_size;

	temp = __get_free_pages(GFP_KERNEL,order);
	__put_user(temp,address);

	AUDIT{
		printk("%s: allocation  address is %p\n",MODNAME,(void*)temp);
	}

	return 0;

bad_size:

	return -1;
}

asmlinkage long sys_deallocate(unsigned long address, int order){

	AUDIT{
		printk("%s: sys_deallocate has been called with params  %p - %d\n",MODNAME,(void*)address,order);
	}

	free_pages(address,order);

	return 0;
}

int init_module(void) {

	unsigned long * p = (unsigned long *) _syscall_table;
	int i,j;
	int ret;

	unsigned long cr0;

	printk("%s: initializing\n",MODNAME);
	printk("%s: SYS_TABLE %px\n",MODNAME, (void*)_syscall_table);
	printk("%s: NI_SYSCALL %px\n",MODNAME, (void*)_ni_syscall);
	j = -1;
	for (i=0; i<256; i++){
		if (p[i] == _ni_syscall){
			printk("%s: table entry %d keeps address %px\n",MODNAME,i,(void*)p[i]);
			j++;
			restore[j] = i;
			if (j == 1) break;
		}
	}

	
	cr0 = read_cr0();
    _write_cr0(cr0 & ~X86_CR0_WP);
	p[restore[0]] = (unsigned long)sys_allocate;
	p[restore[1]] = (unsigned long)sys_deallocate;
	_write_cr0(cr0);
	
	
	printk("%s: new system-call sys_allocate installed on sys-call table entry %d\n",MODNAME,restore[0]);
	printk("%s: new system-call sys_deallocate installed on sys-call table entry %d\n",MODNAME,restore[1]);

	ret = 0;

	return ret;
}


void cleanup_module(void) {

	unsigned long * p = (unsigned long*) _syscall_table;
	unsigned long cr0;
        	
	printk("%s: shutting down\n",MODNAME);
	
	cr0 = read_cr0();
    _write_cr0(cr0 & ~X86_CR0_WP);
	p[restore[0]] = _ni_syscall;
	p[restore[1]] = _ni_syscall;
	_write_cr0(cr0);
	
	printk("%s: sys-call table restored to its original content\n",MODNAME);
	
}

