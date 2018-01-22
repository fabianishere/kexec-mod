#ifndef pr_fmt
#define pr_fmt(fmt) "kexec_mod: " fmt
#endif

#define CONFIG_KEXEC 1
#define CONFIG_KEXEC_CORE 1
#include <linux/kexec.h>

#undef CONFIG_FTRACE_SYSCALLS
#include <linux/syscalls.h>

#undef  VMCOREINFO_SYMBOL
#define VMCOREINFO_SYMBOL(_) do {} while (0)

