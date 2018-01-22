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
	return 0;
}
