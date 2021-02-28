KERNEL ?= /root/linux
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

.PHONY: all module clean prepare

all: module

module: orig
	ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) make -C $(KERNEL)/ M=$(PWD) modules

clean:
	ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) make -C $(KERNEL)/ M=$(PWD) clean
	rm -r orig

prepare:
	ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) make -C $(KERNEL)/ modules_prepare

orig:
	mkdir $@
	ln -s $(KERNEL)/arch/$(ARCH)/kernel/relocate_kernel.S $@/
	ln -s $(KERNEL)/arch/$(ARCH)/kernel/machine_kexec.c $@/
	ln -s $(KERNEL)/kernel/kexec.c $@/
	ln -s $(KERNEL)/kernel/kexec_core.c $@/
	ln -s $(KERNEL)/kernel/kexec_internal.h $@/
