#define pr_fmt(fmt) "kexec_mod: " fmt

#include <asm/uaccess.h>
#include <linux/mm_types.h>
#include <linux/kexec.h>
#include <linux/kallsyms.h>

#include "kexec_compat.h"

static void (*machine_shutdown_ptr)(void);
static void (*kernel_restart_prepare_ptr)(char*);
static int  (*memblock_is_region_memory_ptr)(phys_addr_t, phys_addr_t);
static void (*cpu_hotplug_enable_ptr)(void);
static void (*cpu_do_switch_mm_ptr)(unsigned long, struct mm_struct *);
static void (*__flush_dcache_area_ptr)(void *, size_t);
static void (*migrate_to_reboot_cpu_ptr)(void);

void machine_shutdown(void)
{
	machine_shutdown_ptr();
}

void kernel_restart_prepare(char *cmd)
{
	kernel_restart_prepare_ptr(cmd);
}

int memblock_is_region_memory(phys_addr_t base, phys_addr_t size)
{
	return memblock_is_region_memory_ptr(base, size);
}

bool cpus_are_stuck_in_kernel(void)
{
	return false;
}

void cpu_hotplug_enable(void)
{
	cpu_hotplug_enable_ptr();
}

void compat_cpu_do_switch_mm(unsigned long pgd_phys, struct mm_struct *mm)
{
	cpu_do_switch_mm_ptr(pgd_phys, mm);
}

void cpu_do_switch_mm(unsigned long pgd_phys, struct mm_struct *mm)
{
	compat_cpu_do_switch_mm(pgd_phys, mm);
}

void __flush_dcache_area(void *addr, size_t len)
{
	__flush_dcache_area_ptr(addr, len);
}

void migrate_to_reboot_cpu(void)
{
	migrate_to_reboot_cpu_ptr();
}


int __boot_cpu_mode[2];

static void __init_cpu_boot_mode(void)
{
	/*
	 * Hack to obtain pointer to __boot_mode_cpu
	 * Our approach is to decode the address to __boot_mode_cpu from the instructions
	 * of set_cpu_boot_mode_flag which is exported and references __boot_mode_cpu.
	 */
	u32 *set_cpu_boot_mode_flag_ptr = (void *)kallsyms_lookup_name("set_cpu_boot_mode_flag");
	void *page = (void *) (((unsigned long)set_cpu_boot_mode_flag_ptr) & ~0xFFF);
	u16 lo = (set_cpu_boot_mode_flag_ptr[0] >> 29) & 0x3;
	u16 hi = (set_cpu_boot_mode_flag_ptr[0] >> 4) & 0xFFFF;
	int *__boot_cpu_mode_ptr = page + ((hi << 13) | (lo << 12));

	if (virt_addr_valid(__boot_cpu_mode_ptr)) {
		__boot_cpu_mode[0] = __boot_cpu_mode_ptr[0];
		__boot_cpu_mode[1] = __boot_cpu_mode_ptr[1];

		pr_info("Boot cpu mode: 0x%x 0x%x\n", __boot_cpu_mode[0], __boot_cpu_mode[1]);
	} else {
		pr_warn("Failed to detect boot cpu mode\n");

		__boot_cpu_mode[0] = 0;
		__boot_cpu_mode[1] = 0;
	}
}

static void *ksym(const char *name)
{
	return (void *) kallsyms_lookup_name(name);
}

int kexec_compat_load(void)
{
	if (!(machine_shutdown_ptr = ksym("machine_shutdown"))
#ifdef CONFIG_ARM64
	    || !(cpu_hotplug_enable_ptr = ksym("cpu_hotplug_enable"))
	    || !(cpu_do_switch_mm_ptr = ksym("cpu_do_switch_mm"))
	    || !(__flush_dcache_area_ptr = ksym("__flush_dcache_area"))
#endif
	    || !(migrate_to_reboot_cpu_ptr = ksym("migrate_to_reboot_cpu"))
	    || !(kernel_restart_prepare_ptr = ksym("kernel_restart_prepare")))
		return -ENOENT;
	__init_cpu_boot_mode();
	return 0;
}
