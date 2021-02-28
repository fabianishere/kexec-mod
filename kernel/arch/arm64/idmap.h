#ifndef KEXEC_IDMAP_H
#define KEXEC_IDMAP_H

phys_addr_t kexec_pa_symbol(void *ptr);

void kexec_idmap_setup(void);

void kexec_idmap_install(void);

#endif /* KEXEC_IDMAP_H */
