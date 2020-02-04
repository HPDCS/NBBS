-include system.conf
obj-m += bd_api.o

ifdef SYS_TABLE
FLAGS := -DSYSCALL_TABLE=$(SYS_TABLE)
FLAGS += -DNI_SYSCALL=$(NI_SYSCALL)
endif

all:
	make KCPPFLAGS="$(FLAGS)" -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

init:
	$(eval SYS_TABLE=$(shell sudo cat /proc/kallsyms  | grep sys_call_table | head -n1 | cut -f1 -d' '))
	$(eval NI_SYSCALL=$(shell sudo cat /proc/kallsyms  | grep sys_ni_syscall | cut -f1 -d' '))
	@echo SYS_TABLE=$(SYS_TABLE)
	@echo NI_SYSCALL=$(NI_SYSCALL)
	$(shell echo SYS_TABLE=0x$(SYS_TABLE) > system.conf)
	$(shell echo NI_SYSCALL=0x$(NI_SYSCALL) >> system.conf)
