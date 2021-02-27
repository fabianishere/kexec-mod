#ifndef LINUX_KEXEC_COMPAT_H
#define LINUX_KEXEC_COMPAT_H

/**
 * Load the kexec compatibility layer.
 *
 * @param detect_el2 Attempt to detect the CPU boot mode.
 * @param shim_hyp Attempt to shim the hyp vectors.
 */
int kexec_compat_load(int detect_el2, int shim_hyp);

/**
 * Unload the kexec compatbility layer.
 */
void kexec_compat_unload(void);

/**
 * Run the compatibility layer pre-restart. For instance, this method will
 * conditionally shim the hypervisor vectors.
 */
void kexec_compat_shim(void);

#endif /* LINUX_KEXEC_COMPAT_H */
