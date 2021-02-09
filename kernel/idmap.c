#include <asm/pgtable.h>
#include <asm/mmu_context.h>

#include "idmap.h"

#ifdef CONFIG_ARM64_64K_PAGES
#define BLOCK_SHIFT	PAGE_SHIFT
#define BLOCK_SIZE	PAGE_SIZE
#define TABLE_SHIFT	PMD_SHIFT
#else
#define BLOCK_SHIFT	SECTION_SHIFT
#define BLOCK_SIZE	SECTION_SIZE
#define TABLE_SHIFT	PUD_SHIFT
#endif

#define block_index(addr) (((addr) >> BLOCK_SHIFT) & (PTRS_PER_PTE - 1))
#define block_align(addr) (((addr) >> BLOCK_SHIFT) << BLOCK_SHIFT)

/*
 * Initial memory map attributes.
 */
#ifndef CONFIG_SMP
#define PTE_FLAGS	PTE_TYPE_PAGE | PTE_AF
#define PMD_FLAGS	PMD_TYPE_SECT | PMD_SECT_AF
#else
#define PTE_FLAGS	PTE_TYPE_PAGE | PTE_AF | PTE_SHARED
#define PMD_FLAGS	PMD_TYPE_SECT | PMD_SECT_AF | PMD_SECT_S
#endif

#ifdef CONFIG_ARM64_64K_PAGES
#define MM_MMUFLAGS	PTE_ATTRINDX(MT_NORMAL) | PTE_FLAGS
#else
#define MM_MMUFLAGS	PMD_ATTRINDX(MT_NORMAL) | PMD_FLAGS
#endif

pgd_t kexec_idmap_pg_dir[PTRS_PER_PGD] __attribute__ ((aligned (4096)));
pte_t kexec_idmap_pt[PTRS_PER_PTE] __attribute__ ((aligned (4096)));

struct mm_struct init_mm;

extern void __cpu_soft_restart(unsigned el2_switch,
	unsigned long entry, unsigned long arg0, unsigned long arg1,
	unsigned long arg2);

static void __init_mm(void)
{
	/*
	 * Hack to obtain pointer to swapper_pg_dir (since it is not exported).
	 * However, we can find its physical address in the TTBR1_EL1 register
	 * and convert it to a logical address.
	 */
	u32 val;
	asm volatile("mrs %0, ttbr1_el1" : "=r" (val));
	init_mm.context.id = 0;
	init_mm.pgd = phys_to_virt(val);

}

void kexec_idmap_setup(void)
{
	int i;
	unsigned long pa, pdx;
	void *ptrs[3] = {kexec_idmap_pg_dir, kexec_idmap_pt, __cpu_soft_restart};

	__init_mm();

	/* Clear the idmap page table */
	memset(kexec_idmap_pg_dir, 0, sizeof(kexec_idmap_pg_dir));
	memset(kexec_idmap_pt, 0, sizeof(kexec_idmap_pt));

	/* Identity map necessary pages using 2MB blocks */
	pa = kexec_pa_symbol(kexec_idmap_pt);
	pdx = pgd_index(pa);

	/* Point page directory to page table */
	kexec_idmap_pg_dir[pgd_index(pa)] = pa | PMD_TYPE_TABLE;

	for (i = 0; i < sizeof(ptrs) / sizeof(ptrs[0]); i++) {
		pa = kexec_pa_symbol(ptrs[i]);
		/* We require that all pages belong to the same page directory */
		BUG_ON(pdx != pgd_index(pa));
		kexec_idmap_pt[block_index(pa)] = block_align(pa) | MM_MMUFLAGS;
	}
}

void kexec_idmap_install(void)
{
	cpu_set_reserved_ttbr0();
	flush_tlb_all();

	cpu_do_switch_mm(kexec_pa_symbol(kexec_idmap_pg_dir), &init_mm);
}

/**
 * Resolve the physical address of the specified pointer.
 * We cannot use __pa_symbol for symbols defined in our kernel module, so we need to walk
 * the page manually.
 */
phys_addr_t kexec_pa_symbol(void *ptr)
{
	unsigned long va = (unsigned long) ptr;
	unsigned long page_offset;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *ptep, pte;
	struct page *page = NULL;

	pgd = pgd_offset_k(va);
	if (pgd_none(*pgd) || pgd_bad(*pgd)) {
		return 0;
	}

	pud = pud_offset(pgd , va);
	if (pud_none(*pud) || pud_bad(*pud)) {
		return 0;
	}

	pmd = pmd_offset(pud, va);
	if (pmd_none(*pmd) || pmd_bad(*pmd)) {
		return 0;
	}

	ptep = pte_offset_map(pmd, va);
	if (!ptep) {
		return 0;
	}

	pte = *ptep;
	pte_unmap(ptep);
	page = pte_page(pte);
	page_offset = va & ~PAGE_MASK;
	return page_to_phys(page) | page_offset;
}
