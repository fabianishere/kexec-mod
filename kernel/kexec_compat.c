#define pr_fmt(fmt) "kexec_mod: " fmt

#include <linux/mm_types.h>
#include <linux/kexec.h>
#include <linux/kallsyms.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/virt.h>

#include "kexec_compat.h"

static void (*machine_shutdown_ptr)(void);
static void (*kernel_restart_prepare_ptr)(char*);
static int  (*memblock_is_region_memory_ptr)(phys_addr_t, phys_addr_t);
static void (*cpu_hotplug_enable_ptr)(void);
static void (*cpu_do_switch_mm_ptr)(unsigned long, struct mm_struct *);
static void (*__flush_dcache_area_ptr)(void *, size_t);
static void (*__hyp_set_vectors_ptr)(phys_addr_t);
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

void __hyp_set_vectors(phys_addr_t phys_vector_base)
{
	__hyp_set_vectors_ptr(phys_vector_base);
}

void __hyp_set_vectors_nop(phys_addr_t phys_vector_base)
{}

static void *ksym(const char *name)
{
	return (void *) kallsyms_lookup_name(name);
}

u64 idmap_t0sz = TCR_T0SZ(VA_BITS);
u32 __boot_cpu_mode[2];

/**
 * This function initializes the __boot_cpu_mode variable with that from the kernel.
 * Since it is not exported, this requires some hacks.
 */
static int __init_cpu_boot_mode(void)
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

		pr_info("Detected boot CPU mode: 0x%x 0x%x.\n", __boot_cpu_mode[0], __boot_cpu_mode[1]);
		return 0;
	}

	return -1;
}

static void *__hyp_shim;

/**
 * This function allocates a page which will contain the hypervisor shim.
 *
 * Previously, we set vbar_el2 to point directly to __hyp_shim_vectors.
 * However, we found that sometimes, the shim vectors would span two
 * non-consecutive physical pages, which would cause it to jump into unknown
 * memory.
 *
 * Our solution is to allocate a single page on which we place the hypervisor
 * shim in order to ensure that relative jumps without the MMU still work
 * properly.
 */
static int __init_hyp_shim(void)
{
	extern const unsigned long __hyp_shim_size;
	extern void __hyp_shim_vectors(void);

	__hyp_shim = alloc_pages_exact(__hyp_shim_size, GFP_KERNEL);

	if (!__hyp_shim) {
		return -ENOMEM;
	}

	memcpy(__hyp_shim, __hyp_shim_vectors, __hyp_shim_size);

	pr_info("Hypervisor shim created at 0x%llx [%lu bytes].\n", virt_to_phys(__hyp_shim), __hyp_shim_size);
	return 0;
}

int kexec_compat_load(int detect_el2, int shim_hyp)
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

	/* Find boot CPU mode */
	__boot_cpu_mode[0] = BOOT_CPU_MODE_EL1;
	__boot_cpu_mode[1] = BOOT_CPU_MODE_EL1;

	if (!detect_el2) {
		pr_info("EL2 kexec not supported.\n");
	} else if (__init_cpu_boot_mode() < 0) {
		pr_warn("Failed to detect boot CPU mode.\n");
	}

	/* Enable shimming the hypervisor vectors */
	__hyp_set_vectors_ptr = __hyp_set_vectors_nop;
	if (shim_hyp) {
		pr_info("Enabling shim for hypervisor vectors.\n");

		if (__init_hyp_shim() < 0) {
			pr_err("Failed to initialize hypervisor shim.\n");
		} else if (detect_el2 && !(__hyp_set_vectors_ptr = ksym("__hyp_set_vectors"))) {
			pr_err("Not able to shim hypervisor vectors.\n");
			__hyp_set_vectors_ptr = __hyp_set_vectors_nop;
		} else if (!detect_el2) {
			pr_warn("Hypervisor shim unnecessary without EL2 detection.\n");
		}
	}
	return 0;
}

void kexec_compat_unload(void)
{
	extern const unsigned long __hyp_shim_size;

	free_pages_exact(__hyp_shim, __hyp_shim_size);
	__hyp_shim = NULL;
}

void kexec_compat_shim(void)
{
	__hyp_set_vectors(virt_to_phys(__hyp_shim));
}
