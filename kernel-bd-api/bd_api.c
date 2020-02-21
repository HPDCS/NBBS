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
#include <linux/version.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>
#include <linux/random.h>

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


#include "../benchmarks/TB_linux-scalability/main.h"
#include "../benchmarks/TB_threadtest/main.h"
#include "../benchmarks/TB_fixed-size/main.h"
#include "../benchmarks/TB_cached_allocation/main.h"

#define NUM_BENCHMARKS 5

int restore[NUM_BENCHMARKS] = {[0 ... 1] -1};
unsigned long _force_order_;

/* please take the two values below from the system map */
unsigned long _syscall_table = SYSCALL_TABLE;
unsigned long _ni_syscall = NI_SYSCALL; 


#define AUDIT if (0)
#define MAX_BD_ORDER 12

int valid = 0;
int last_type = -1;

static inline void _write_cr0(unsigned long val)
{
	asm volatile("mov %0, %%cr0" : : "r" (val), "m" (_force_order_));
}


__SYSCALL_DEFINEx(4, _linuxscalability, int, order, unsigned long*, all, unsigned long*, fai, unsigned long*, fre)
{
	unsigned long long allocs = 0, failures = 0, frees = 0;
	AUDIT{
		printk("%s: sys_allocate has been called with param  %d\n",MODNAME,order);
	}

	if(order > MAX_BD_ORDER) goto bad_size;

	
	linux_scalability(order, &allocs, &failures, &frees);
	printk("%s: allocation address is %llu %llu %llu\n",MODNAME, allocs, failures, frees);
	
	__put_user(allocs,all);
	__put_user(failures,fai);
	__put_user(frees,fre);

	return 0;

bad_size:

	return -1;
}

__SYSCALL_DEFINEx(5, _threadtest, int, order, unsigned int, number_of_processes, unsigned long*, all, unsigned long*, fai, unsigned long*, fre)
{
	unsigned long long allocs = 0, failures = 0, frees = 0;
	AUDIT{
		printk("%s: sys_allocate has been called with param  %d\n",MODNAME,order);
	}

	if(order > MAX_BD_ORDER) goto bad_size;

	
	threadtest(order, number_of_processes, &allocs, &failures, &frees);
	printk("%s: allocation address is %llu %llu %llu\n",MODNAME, allocs, failures, frees);
	
	__put_user(allocs,all);
	__put_user(failures,fai);
	__put_user(frees,fre);

	return 0;

bad_size:

	return -1;
}

__SYSCALL_DEFINEx(5, _costantoccupancy, int, order, unsigned int, number_of_processes, unsigned long*, all, unsigned long*, fai, unsigned long*, fre)
{
	unsigned long long allocs = 0, failures = 0, frees = 0;
	AUDIT{
		printk("%s: sys_allocate has been called with param  %d\n",MODNAME,order);
	}

	if(order > MAX_BD_ORDER) goto bad_size;

	
	fixedsize(order, number_of_processes, &allocs, &failures, &frees);
	printk("%s: allocation address is %llu %llu %llu\n",MODNAME, allocs, failures, frees);
	
	__put_user(allocs,all);
	__put_user(failures,fai);
	__put_user(frees,fre);

	return 0;

bad_size:

	return -1;
}

__SYSCALL_DEFINEx(4, _cachedallocation, int, order, unsigned long*, all, unsigned long*, fai, unsigned long*, fre)
{
	unsigned long long allocs = 0, failures = 0, frees = 0;
	AUDIT{
		printk("%s: sys_allocate has been called with param  %d\n",MODNAME,order);
	}

	if(order > MAX_BD_ORDER) goto bad_size;

	
	cached_allocation(order, &allocs, &failures, &frees);
	printk("%s: allocation address is %llu %llu %llu\n",MODNAME, allocs, failures, frees);
	
	__put_user(allocs,all);
	__put_user(failures,fai);
	__put_user(frees,fre);

	return 0;

bad_size:

	return -1;
}



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
static unsigned long sys_linuxscalability_ptr = (unsigned long) __x64_sys_linuxscalability;
static unsigned long sys_threadtest_ptr       = (unsigned long) __x64_sys_threadtest;
static unsigned long sys_costantoccupancy_ptr = (unsigned long) __x64_sys_costantoccupancy;
static unsigned long sys_cachedallocation_ptr = (unsigned long) __x64_sys_cachedallocation;
#else
static unsigned long sys_linuxscalability_ptr = (unsigned long) sys_linuxscalability;
static unsigned long sys_threadtest_ptr       = (unsigned long) sys_threadtest;
static unsigned long sys_costantoccupancy_ptr = (unsigned long) sys_costantoccupancy;
static unsigned long sys_cachedallocation_ptr = (unsigned long) sys_cachedallocation;
#endif


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
			if (j == NUM_BENCHMARKS-1) break;
		}
	}

	
	cr0 = read_cr0();
    _write_cr0(cr0 & ~X86_CR0_WP);
	p[restore[0]] = (unsigned long)sys_linuxscalability_ptr;
	p[restore[1]] = (unsigned long)sys_threadtest_ptr;
	p[restore[2]] = (unsigned long)sys_costantoccupancy_ptr;
	p[restore[3]] = (unsigned long)sys_cachedallocation_ptr;
	_write_cr0(cr0);
	
	
	printk("%s: sys_linuxscalability_ptr installed on sys-call table entry %d\n",MODNAME,restore[0]);
	printk("%s: sys_threadtest_ptr       installed on sys-call table entry %d\n",MODNAME,restore[1]);
	printk("%s: sys_costantoccupancy_ptr installed on sys-call table entry %d\n",MODNAME,restore[2]);
	printk("%s: sys_chacedallocation_ptr installed on sys-call table entry %d\n",MODNAME,restore[3]);

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
	p[restore[2]] = _ni_syscall;
	p[restore[3]] = _ni_syscall;
	_write_cr0(cr0);
	
	printk("%s: sys-call table restored to its original content\n",MODNAME);
	
}

