#ifndef LINUX_KEXEC_COMPAT_H
#define LINUX_KEXEC_COMPAT_H

/**
 * Load the kexec compatibility layer.
 */
int kexec_compat_load(void);

/**
 * Unload the kexec compatbility layer.
 */
void kexec_compat_unload(void);

#endif /* LINUX_KEXEC_COMPAT_H */
