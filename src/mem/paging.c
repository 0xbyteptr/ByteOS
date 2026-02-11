#include "paging.h"
#include "serial/serial.h"
#include "alloc.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Assumption: physical memory is directly mapped into the kernel virtual
 * address space at a fixed offset. Many hobby kernels place the direct map
 * at the kernel base (higher-half). We use this constant to convert
 * physical addresses (from page table entries) to kernel virtual addresses
 * so we can edit PTEs.
 *
 * If this assumption is incorrect for the current boot environment you may
 * need to adjust PHYS_TO_VIRT below to the correct phys->virt offset.
 */
#define KERNEL_VIRT_BASE 0xffffffff80000000ULL
#define PHYS_TO_VIRT(paddr) ((void *)((uint64_t)(paddr) + KERNEL_VIRT_BASE))

static inline uint64_t read_cr3(void) {
  uint64_t v;
  asm volatile("mov %%cr3, %0" : "=r"(v));
  return v;
}

/* invalidate single page from TLB */
static inline void invlpg(void *addr) {
  asm volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

/* Walk page tables and set U bit for each 4KiB page in the range.
 * Returns 0 on success, -1 on any error (missing mapping).
 */
int paging_set_user(void *va, size_t size) {
  if (!va || size == 0)
    return -1;
  uint64_t start = (uint64_t)va;
  uint64_t end = start + size;

  uint64_t cr3 = read_cr3();
  uint64_t pml4_phys = cr3 & ~0xFFFULL;
  uint64_t *pml4 = (uint64_t *)PHYS_TO_VIRT(pml4_phys);

  for (uint64_t addr = start; addr < end; addr = (addr & ~0xFFFULL) + 0x1000) {
    size_t pml4_idx = (addr >> 39) & 0x1FF;
    size_t pdpt_idx = (addr >> 30) & 0x1FF;
    size_t pd_idx = (addr >> 21) & 0x1FF;
    size_t pt_idx = (addr >> 12) & 0x1FF;

    uint64_t pml4e = pml4[pml4_idx];
    if (!(pml4e & 1)) {
      serial_puts("paging: pml4e not present\n");
      return -1;
    }

    uint64_t *pdpt = (uint64_t *)PHYS_TO_VIRT(pml4e & ~0xFFFULL);
    uint64_t pdpte = pdpt[pdpt_idx];
    if (!(pdpte & 1)) {
      serial_puts("paging: pdpte not present\n");
      return -1;
    }

    /* 1 GiB large page? set user bit on pdpte if so */
    if (pdpte & (1ULL << 7)) {
      pdpt[pdpt_idx] = pdpte | 0x4ULL;
      invlpg((void *)addr);
      continue;
    }

    uint64_t *pdt = (uint64_t *)PHYS_TO_VIRT(pdpte & ~0xFFFULL);
    uint64_t pde = pdt[pd_idx];
    if (!(pde & 1)) {
      serial_puts("paging: pde not present\n");
      return -1;
    }

    /* 2 MiB large page? set user bit on pde if so */
    if (pde & (1ULL << 7)) {
      pdt[pd_idx] = pde | 0x4ULL;
      invlpg((void *)addr);
      continue;
    }

    uint64_t *pt = (uint64_t *)PHYS_TO_VIRT(pde & ~0xFFFULL);
    uint64_t pte = pt[pt_idx];
    if (!(pte & 1)) {
      serial_puts("paging: pte not present\n");
      return -1;
    }

    pt[pt_idx] = pte | 0x4ULL; /* set user bit */
    invlpg((void *)addr);
  }

  return 0;
}

/* Get physical address of virtual address */
uint64_t get_phys(uint64_t va) {
  uint64_t cr3 = read_cr3();
  uint64_t pml4_phys = cr3 & ~0xFFFULL;
  uint64_t *pml4 = (uint64_t *)PHYS_TO_VIRT(pml4_phys);

  uint64_t pml4_idx = (va >> 39) & 0x1FF;
  uint64_t pdpt_idx = (va >> 30) & 0x1FF;
  uint64_t pd_idx = (va >> 21) & 0x1FF;
  uint64_t pt_idx = (va >> 12) & 0x1FF;

  uint64_t pml4e = pml4[pml4_idx];
  if (!(pml4e & 1)) return 0;

  uint64_t *pdpt = (uint64_t *)PHYS_TO_VIRT(pml4e & ~0xFFFULL);
  uint64_t pdpte = pdpt[pdpt_idx];
  if (!(pdpte & 1)) return 0;

  if (pdpte & (1ULL << 7)) {
    return (pdpte & ~0xFFFULL) | (va & 0x3FFFFFFF);
  }

  uint64_t *pdt = (uint64_t *)PHYS_TO_VIRT(pdpte & ~0xFFFULL);
  uint64_t pde = pdt[pd_idx];
  if (!(pde & 1)) return 0;

  if (pde & (1ULL << 7)) {
    return (pde & ~0xFFFULL) | (va & 0x1FFFFF);
  }

  uint64_t *pt = (uint64_t *)PHYS_TO_VIRT(pde & ~0xFFFULL);
  uint64_t pte = pt[pt_idx];
  if (!(pte & 1)) return 0;

  return pte & ~0xFFFULL;
}

/* Allocate a physical page */
uint64_t alloc_page(void) {
  void *page = kmalloc(0x2000);
  if (!page) {
    serial_puts("kmalloc failed in alloc_page\n");
    return 0;
  }
  uint64_t va = (uint64_t)page;
  uint64_t aligned_va = (va + 0xFFF) & ~0xFFFULL;
  memset((void *)aligned_va, 0, 0x1000);
  return get_phys(aligned_va);
}

/* Map a kernel virtual address range to user virtual address */
int paging_map_user_va(uint64_t user_va, uint64_t kernel_va, size_t size) {
  if (!user_va || !kernel_va || size == 0)
    return -1;

  uint64_t cr3 = read_cr3();
  uint64_t pml4_phys = cr3 & ~0xFFFULL;
  uint64_t *pml4 = (uint64_t *)PHYS_TO_VIRT(pml4_phys);

  for (uint64_t addr = user_va; addr < user_va + size; addr += 0x1000) {
    uint64_t kaddr = kernel_va + (addr - user_va);

    // Get the physical address of kaddr
    uint64_t phys = 0;
    // To get phys of kaddr, since it's mapped, we can walk the page tables for kaddr
    uint64_t pml4_idx = (kaddr >> 39) & 0x1FF;
    uint64_t pdpt_idx = (kaddr >> 30) & 0x1FF;
    uint64_t pd_idx = (kaddr >> 21) & 0x1FF;
    uint64_t pt_idx = (kaddr >> 12) & 0x1FF;

    uint64_t pml4e = pml4[pml4_idx];
    if (!(pml4e & 1)) continue;
    uint64_t *pdpt = (uint64_t *)PHYS_TO_VIRT(pml4e & ~0xFFFULL);
    uint64_t pdpte = pdpt[pdpt_idx];
    if (!(pdpte & 1)) continue;
    if (pdpte & (1ULL << 7)) {
      phys = (pdpte & ~0xFFFULL) | (kaddr & 0x3FFFFFFF);
    } else {
      uint64_t *pdt = (uint64_t *)PHYS_TO_VIRT(pdpte & ~0xFFFULL);
      uint64_t pde = pdt[pd_idx];
      if (!(pde & 1)) continue;
      if (pde & (1ULL << 7)) {
        phys = (pde & ~0xFFFULL) | (kaddr & 0x1FFFFF);
      } else {
        uint64_t *pt = (uint64_t *)PHYS_TO_VIRT(pde & ~0xFFFULL);
        uint64_t pte = pt[pt_idx];
        if (!(pte & 1)) continue;
        phys = pte & ~0xFFFULL;
      }
    }

    if (!phys) continue;

    serial_puts("mapping addr ");
    serial_puthex64(addr);
    serial_puts(" phys ");
    serial_puthex64(phys);
    serial_puts("\n");

    // Now map user_va to phys with user bit
    uint64_t u_pml4_idx = (addr >> 39) & 0x1FF;
    uint64_t u_pdpt_idx = (addr >> 30) & 0x1FF;
    uint64_t u_pd_idx = (addr >> 21) & 0x1FF;
    uint64_t u_pt_idx = (addr >> 12) & 0x1FF;

    // Ensure page tables exist for user va
    if (!pml4[u_pml4_idx]) {
      // Allocate page table
      uint64_t pt_phys = alloc_page();
      if (!pt_phys) return -1;
      pml4[u_pml4_idx] = pt_phys | 0x3; // present, writable
      invlpg((void *)addr);
      uint64_t *new_pdpt = (uint64_t *)PHYS_TO_VIRT(pt_phys);
      memset(new_pdpt, 0, 0x1000);
    }
    uint64_t *u_pdpt = (uint64_t *)PHYS_TO_VIRT(pml4[u_pml4_idx] & ~0xFFFULL);
    if (!u_pdpt[u_pdpt_idx]) {
      uint64_t pt_phys = alloc_page();
      if (!pt_phys) return -1;
      u_pdpt[u_pdpt_idx] = pt_phys | 0x3;
      invlpg((void *)addr);
      uint64_t *new_pdt = (uint64_t *)PHYS_TO_VIRT(pt_phys);
      memset(new_pdt, 0, 0x1000);
    }
    uint64_t *u_pdt = (uint64_t *)PHYS_TO_VIRT(u_pdpt[u_pdpt_idx] & ~0xFFFULL);
    if (!u_pdt[u_pd_idx]) {
      uint64_t pt_phys = alloc_page();
      if (!pt_phys) return -1;
      u_pdt[u_pd_idx] = pt_phys | 0x3;
      invlpg((void *)addr);
      uint64_t *new_pt = (uint64_t *)PHYS_TO_VIRT(pt_phys);
      memset(new_pt, 0, 0x1000);
    }
    uint64_t *u_pt = (uint64_t *)PHYS_TO_VIRT(u_pdt[u_pd_idx] & ~0xFFFULL);
    u_pt[u_pt_idx] = phys | 0x7; // present, writable, user
    invlpg((void *)addr);
  }

  return 0;
}
