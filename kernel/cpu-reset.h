/*
 * CPU reset routines
 *
 * Copyright (C) 2015 Huawei Futurewei Technologies.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ARM64_CPU_RESET_H
#define _ARM64_CPU_RESET_H

#include <asm/pgtable.h>
#include <asm/sysreg.h>
#include <asm/virt.h>

#include "idmap.h"

extern void __hyp_shim_vectors(void);

void __cpu_soft_restart(unsigned el2_switch,
	unsigned long entry, unsigned long arg0, unsigned long arg1,
	unsigned long arg2);

static void __noreturn cpu_soft_restart(unsigned long el2_switch,
	unsigned long entry, unsigned long arg0, unsigned long arg1,
	unsigned long arg2)
{
	typeof(__cpu_soft_restart) *restart;

	el2_switch = el2_switch && !is_kernel_in_hyp_mode() &&
		is_hyp_mode_available();
	
	restart = (void *)kexec_pa_symbol(__cpu_soft_restart);

	/* Shim the hypervisor vectors for HYP_SOFT_RESTART support */
	__hyp_set_vectors(kexec_pa_symbol(__hyp_shim_vectors));

	/* Install identity mapping */
	kexec_idmap_install();

	restart(el2_switch, entry, arg0, arg1, arg2);
	unreachable();
}

#endif
