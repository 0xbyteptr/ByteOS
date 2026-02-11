
#include "paging.h"
#include "alloc.h"
#include "serial/serial.h"
#include "utils/log.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// --- Early boot bump allocator for page tables ---
static uint64_t early_bump_base = 0;
static uint64_t early_bump_top = 0;
static int heap_mapped = 0;

void paging_set_early_bump(uint64_t base, uint64_t size) {
  early_bump_base = base;
  early_bump_top = base + size;
}

void paging_mark_heap_mapped(void) {
  heap_mapped = 1;
}

/* Kernel higher-half mapping base (adjust if your kernel uses different offset) */
#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000ULL
#define PHYS_TO_VIRT(paddr) ((void*) ((uint64_t) (paddr) + KERNEL_VIRT_BASE))
#define VIRT_TO_PHYS(vaddr) ((uint64_t) (vaddr) - KERNEL_VIRT_BASE)

/* Recursive paging constants */
#define PML4_RECURSIVE_INDEX 510ULL
#define PML4_RECURSIVE_VADDR 0xFFFFFF7FBFC00000ULL /* PML4[510] → PML4 itself */

/* Invalidate TLB for one page */
static inline void invlpg(void* addr)
{
  asm volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

static inline uint64_t read_cr3(void)
{
  uint64_t cr3;
  asm volatile("mov %%cr3, %0" : "=r"(cr3));
  return cr3;
}

void paging_init(void)
{
  log("paging_init: starting");

  // Identity map the early bump allocator region so memset((void*)phys,...) is safe
  extern uint64_t _bss_end;
  serial_puts("paging_init: identity-mapping early bump region\n");
  uint64_t cr3 = read_cr3();
  uint64_t bump_phys_start = (uint64_t)&_bss_end;
  uint64_t bump_phys_end = bump_phys_start + 0x100000; // 1 MiB
  uint64_t* pml4 = (uint64_t*)(cr3 & ~0xFFFULL);
  for (uint64_t pa = bump_phys_start & ~0xFFFULL; pa < bump_phys_end; pa += 0x1000) {
    serial_puts("paging_init: mapping pa=0x"); serial_puthex64(pa); serial_puts("\n");
    uint64_t va = pa; // identity mapping
    uint64_t pml4_idx = (va >> 39) & 0x1FF;
    uint64_t pdpt_idx = (va >> 30) & 0x1FF;
    uint64_t pd_idx   = (va >> 21) & 0x1FF;
    uint64_t pt_idx   = (va >> 12) & 0x1FF;
    // PML4
    serial_puts("  PML4 idx: "); serial_puthex64(pml4_idx); serial_puts(" entry: "); serial_puthex64(pml4[pml4_idx]); serial_puts("\n");
    if (!(pml4[pml4_idx] & 1)) {
      serial_puts("  New PML4 entry\n");
      pml4[pml4_idx] = pa | 0x3;
    }
    uint64_t* pdpt = get_pdpt(va);
    serial_puts("  PDPT idx: "); serial_puthex64(pdpt_idx); serial_puts(" entry: "); serial_puthex64(pdpt[pdpt_idx]); serial_puts("\n");
    if (!(pdpt[pdpt_idx] & 1)) {
      serial_puts("  New PDPT entry\n");
      pdpt[pdpt_idx] = pa | 0x3;
    }
    uint64_t* pd = get_pd(va);
    serial_puts("  PD idx: "); serial_puthex64(pd_idx); serial_puts(" entry: "); serial_puthex64(pd[pd_idx]); serial_puts("\n");
    if (!(pd[pd_idx] & 1)) {
      serial_puts("  New PD entry\n");
      pd[pd_idx] = pa | 0x3;
    }
    uint64_t* pt = get_pt(va);
    serial_puts("  PT idx: "); serial_puthex64(pt_idx); serial_puts("\n");
    pt[pt_idx] = pa | 0x3;
  }
  serial_puts("paging_init: identity-mapped early bump region\n");
  serial_puts("paging_init: cr3 = ");
  serial_puthex64(cr3);
  serial_puts("\n");
  uint64_t pml4_phys = cr3 & ~0xFFFULL;
  serial_puts("paging_init: pml4_phys = ");
  serial_puthex64(pml4_phys);
  serial_puts("\n");
  uint64_t* pml4_virt = (uint64_t*) pml4_phys;
  serial_puts("paging_init: pml4_virt = ");
  serial_puthex64((uint64_t) pml4_virt);
  serial_puts("\n");
  log("paging_init: setting up recursive mapping");
  serial_puts("paging_init: about to set recursive entry\n");
  serial_puts("pml4_virt = 0x");
  serial_puthex64((uint64_t) pml4_virt);
  serial_puts(", PML4_RECURSIVE_INDEX = ");
  serial_putdec(PML4_RECURSIVE_INDEX);
  serial_puts(", pml4_phys = 0x");
  serial_puthex64(pml4_phys);
  serial_puts("\n");
  // Check if the PML4 page is writable at its physical address
  uint64_t test = pml4_virt[PML4_RECURSIVE_INDEX];
  serial_puts("paging_init: test read OK\n");
  pml4_virt[PML4_RECURSIVE_INDEX] = pml4_phys | 0x3; /* present + writable */
  serial_puts("paging_init: set recursive entry\n");
  // Extra debug: print value before write
  serial_puts("paging_init: recursive entry before write: ");
  serial_puthex64(pml4_virt[PML4_RECURSIVE_INDEX]);
  serial_puts("\n");
  pml4_virt[PML4_RECURSIVE_INDEX] = pml4_phys | 0x3; /* present + writable */
  serial_puts("paging_init: recursive entry after write:  ");
  serial_puthex64(pml4_virt[PML4_RECURSIVE_INDEX]);
  serial_puts("\n");
  serial_puts("paging_init: expected value:              ");
  serial_puthex64(pml4_phys | 0x3);
  serial_puts("\n");
  // Verify write
  if (pml4_virt[PML4_RECURSIVE_INDEX] != (pml4_phys | 0x3))
  {
    serial_puts("paging_init: ERROR: could not write recursive entry!\n");
    serial_puts("  Address: ");
    serial_puthex64((uint64_t)&pml4_virt[PML4_RECURSIVE_INDEX]);
    serial_puts("\n  PML4 virt base: ");
    serial_puthex64((uint64_t)pml4_virt);
    serial_puts("\n  PML4 phys base: ");
    serial_puthex64(pml4_phys);
    serial_puts("\n  Index: ");
    serial_putdec(PML4_RECURSIVE_INDEX);
    serial_puts("\n");
    for (;;)
      asm volatile("hlt");
  }
  log("paging_init: recursive mapping set in PML4");
  invlpg((void*) PML4_RECURSIVE_VADDR);
  serial_puts("paging_init: recursive mapping set up\n");
  log("paging_init: done");
}

/* Helper to get recursive entry for a given level */
static inline uint64_t* get_pml4(void)
{
  return (uint64_t*) PML4_RECURSIVE_VADDR;
}

/* Get PDPT for a given vaddr */
static inline uint64_t* get_pdpt(uint64_t vaddr)
{
  uint64_t pml4_idx = (vaddr >> 39) & 0x1FF;
  return (uint64_t*) (0xFFFFFF7F4000000000ULL | (pml4_idx << 39));
}

/* Get PD for a given vaddr */
static inline uint64_t* get_pd(uint64_t vaddr)
{
  uint64_t pml4_idx = (vaddr >> 39) & 0x1FF;
  uint64_t pdpt_idx = (vaddr >> 30) & 0x1FF;
  return (uint64_t*) (0xFFFFFF7F0000000000ULL | (pml4_idx << 39) | (pdpt_idx << 30));
}

/* Get PT for a given vaddr */
static inline uint64_t* get_pt(uint64_t vaddr)
{
  uint64_t pml4_idx = (vaddr >> 39) & 0x1FF;
  uint64_t pdpt_idx = (vaddr >> 30) & 0x1FF;
  uint64_t pd_idx   = (vaddr >> 21) & 0x1FF;
  return (uint64_t*) (0xFFFFFF7E0000000000ULL | (pml4_idx << 39) | (pdpt_idx << 30) |
                      (pd_idx << 21));
}

/* Set the USER bit (bit 2) on all 4KiB pages covering the range [va, va+size) */
int paging_set_user(void* va, size_t size)
{
  if (!va || size == 0)
  {
    serial_puts("paging_set_user: invalid range\n");
    return -1;
  }

  uint64_t  start = (uint64_t) va;
  uint64_t  end   = start + size;
  uint64_t* pml4  = get_pml4();

  for (uint64_t addr = start; addr < end; addr += 0x1000)
  {
    uint64_t pml4_idx = (addr >> 39) & 0x1FF;
    uint64_t pdpt_idx = (addr >> 30) & 0x1FF;
    uint64_t pd_idx   = (addr >> 21) & 0x1FF;
    uint64_t pt_idx   = (addr >> 12) & 0x1FF;

    uint64_t pml4e = pml4[pml4_idx];
    if (!(pml4e & 1))
    {
      serial_puts("paging_set_user: PML4E not present at ");
      serial_puthex64(addr);
      serial_puts("\n");
      return -1;
    }

    uint64_t* pdpt  = get_pdpt(addr);
    uint64_t  pdpte = pdpt[pdpt_idx];
    if (!(pdpte & 1))
    {
      serial_puts("paging_set_user: PDPTE not present at ");
      serial_puthex64(addr);
      serial_puts("\n");
      return -1;
    }

    /* 1 GiB page */
    if (pdpte & (1ULL << 7))
    {
      pdpt[pdpt_idx] |= 0x4;  // set U bit
      invlpg((void*) addr);
      continue;
    }

    uint64_t* pd  = get_pd(addr);
    uint64_t  pde = pd[pd_idx];
    if (!(pde & 1))
    {
      serial_puts("paging_set_user: PDE not present at ");
      serial_puthex64(addr);
      serial_puts("\n");
      return -1;
    }

    /* 2 MiB page */
    if (pde & (1ULL << 7))
    {
      pd[pd_idx] |= 0x4;  // set U bit
      invlpg((void*) addr);
      continue;
    }

    uint64_t* pt  = get_pt(addr);
    uint64_t  pte = pt[pt_idx];
    if (!(pte & 1))
    {
      serial_puts("paging_set_user: PTE not present at ");
      serial_puthex64(addr);
      serial_puts("\n");
      return -1;
    }

    pt[pt_idx] |= 0x4;  // set U bit
    invlpg((void*) addr);
  }

  return 0;
}

/* Get physical address from virtual address (returns 0 on failure) */
uint64_t paging_get_phys(uint64_t va)
{
  uint64_t* pml4 = get_pml4();

  uint64_t pml4_idx = (va >> 39) & 0x1FF;
  uint64_t pdpt_idx = (va >> 30) & 0x1FF;
  uint64_t pd_idx   = (va >> 21) & 0x1FF;
  uint64_t pt_idx   = (va >> 12) & 0x1FF;

  uint64_t pml4e = pml4[pml4_idx];
  if (!(pml4e & 1))
    return 0;

  uint64_t* pdpt  = get_pdpt(va);
  uint64_t  pdpte = pdpt[pdpt_idx];
  if (!(pdpte & 1))
    return 0;

  if (pdpte & (1ULL << 7))
  {
    return (pdpte & ~0x3FFFFFULL) | (va & 0x3FFFFFFF);
  }

  uint64_t* pd  = get_pd(va);
  uint64_t  pde = pd[pd_idx];
  if (!(pde & 1))
    return 0;

  if (pde & (1ULL << 7))
  {
    return (pde & ~0x1FFFFFULL) | (va & 0x1FFFFF);
  }

  uint64_t* pt  = get_pt(va);
  uint64_t  pte = pt[pt_idx];
  if (!(pte & 1))
    return 0;

  return pte & ~0xFFFULL;
}

/* Allocate and zero a new physical page, return its **physical** address */
uint64_t alloc_page(void)
{
  /* kmalloc usually returns virtual address in kernel space */
  if (!heap_mapped && early_bump_base && early_bump_base < early_bump_top) {
    // Early boot: use bump allocator
    uint64_t phys = (early_bump_base + 0xFFF) & ~0xFFFULL;
    if (phys + 0x1000 > early_bump_top) {
      serial_puts("alloc_page: early bump exhausted\n");
      return 0;
    }
    early_bump_base = phys + 0x1000;
    // Use identity mapping for memset before higher-half is mapped
    memset((void*)phys, 0, 0x1000);
    serial_puts("[DEBUG] alloc_page: early bump returned "); serial_puthex64(phys); serial_puts("\n");
    return phys;
  }
  // Normal: use kmalloc
  serial_puts("[DEBUG] alloc_page: calling kmalloc\n");
  void* va = kmalloc(0x2000);
  serial_puts("[DEBUG] alloc_page: kmalloc returned "); serial_puthex64((uint64_t)va); serial_puts("\n");
  if (!va)
  {
    serial_puts("alloc_page: kmalloc failed\n");
    return 0;
  }

  /* Align to page boundary */
  uintptr_t aligned_va = ((uintptr_t) va + 0xFFF) & ~0xFFFULL;
  serial_puts("[DEBUG] alloc_page: aligned_va = "); serial_puthex64(aligned_va); serial_puts("\n");
  memset((void*) aligned_va, 0, 0x1000);
  serial_puts("[DEBUG] alloc_page: memset done\n");

  uint64_t phys = paging_get_phys(aligned_va);
  serial_puts("[DEBUG] alloc_page: paging_get_phys returned "); serial_puthex64(phys); serial_puts("\n");
  if (!phys)
  {
    serial_puts("alloc_page: get_phys failed for ");
    serial_puthex64(aligned_va);
    serial_puts("\n");
    kfree(va);
    return 0;
  }

  return phys;
}

/* Map kernel virtual address range to user virtual address range */
int paging_map_user_va(uint64_t user_va, uint64_t kernel_va, size_t size)
{
  if (user_va == 0 || kernel_va == 0 || size == 0 || (user_va & 0xFFF) != 0 ||
      (kernel_va & 0xFFF) != 0)
  {
    serial_puts("paging_map_user_va: invalid/misaligned arguments\n");
    return -1;
  }

  uint64_t* pml4 = get_pml4();

  for (uint64_t offset = 0; offset < size; offset += 0x1000)
  {
    uint64_t dst_va = user_va + offset;
    uint64_t src_va = kernel_va + offset;

    uint64_t phys = paging_get_phys(src_va);
    if (!phys)
    {
      serial_puts("paging_map_user_va: no physical mapping for kernel VA 0x");
      serial_puthex64(src_va);
      serial_puts("\n");
      return -1;
    }

    uint64_t pml4_idx = (dst_va >> 39) & 0x1FF;
    uint64_t pdpt_idx = (dst_va >> 30) & 0x1FF;
    uint64_t pd_idx   = (dst_va >> 21) & 0x1FF;
    uint64_t pt_idx   = (dst_va >> 12) & 0x1FF;

    // PML4E
    if (!(pml4[pml4_idx] & 1))
    {
      uint64_t new_phys = alloc_page();
      if (!new_phys)
      {
        serial_puts("paging_map_user_va: alloc failed for PML4E\n");
        return -1;
      }
      memset(PHYS_TO_VIRT(new_phys), 0, 0x1000);
      pml4[pml4_idx] = new_phys | 0x7;  // P + W + U
      invlpg((void*) dst_va);
    }

    // PDPTE
    uint64_t* pdpt = get_pdpt(dst_va);
    if (!(pdpt[pdpt_idx] & 1))
    {
      uint64_t new_phys = alloc_page();
      if (!new_phys)
      {
        serial_puts("paging_map_user_va: alloc failed for PDPTE\n");
        return -1;
      }
      memset(PHYS_TO_VIRT(new_phys), 0, 0x1000);
      pdpt[pdpt_idx] = new_phys | 0x7;
      invlpg((void*) dst_va);
    }

    // PDE
    uint64_t* pd = get_pd(dst_va);
    if (!(pd[pd_idx] & 1))
    {
      uint64_t new_phys = alloc_page();
      if (!new_phys)
      {
        serial_puts("paging_map_user_va: alloc failed for PDE\n");
        return -1;
      }
      memset(PHYS_TO_VIRT(new_phys), 0, 0x1000);
      pd[pd_idx] = new_phys | 0x7;
      invlpg((void*) dst_va);
    }

    // PTE – final mapping, user accessible
    uint64_t* pt = get_pt(dst_va);
    pt[pt_idx]   = phys | 0x7;  // Present + Writable + User
    invlpg((void*) dst_va);

    // Optional: per-page debug (remove in production)
    serial_puts("paging_map_user_va: mapped user VA 0x");
    serial_puthex64(dst_va);
    serial_puts(" -> PA 0x");
    serial_puthex64(phys);
    serial_puts("\n");
  }

  return 0;
}
void paging_identity_map_kernel_heap(void)
{
  serial_puts("[DEBUG] paging_identity_map_kernel_heap: start\n");
  uint64_t heap_start = KERNEL_HEAP_START;
  uint64_t heap_end   = heap_start + KERNEL_HEAP_SIZE;
  for (uint64_t va = heap_start; va < heap_end; va += 0x1000)
  {
    uint64_t  pa       = va - 0xFFFFFFFF80000000ULL;  // physical address
    uint64_t* pml4     = get_pml4();
    uint64_t  pml4_idx = (va >> 39) & 0x1FF;
    uint64_t  pdpt_idx = (va >> 30) & 0x1FF;
    uint64_t  pd_idx   = (va >> 21) & 0x1FF;
    uint64_t  pt_idx   = (va >> 12) & 0x1FF;
    serial_puts("[DEBUG] heap map: va=0x"); serial_puthex64(va); serial_puts(" pa=0x"); serial_puthex64(pa); serial_puts("\n");
    if (!(pml4[pml4_idx] & 1))
    {
      serial_puts("[DEBUG] heap: new PML4\n");
      uint64_t new_pt_phys = alloc_page();
      if (!new_pt_phys) {
        serial_puts("[DEBUG] heap: alloc_page failed for PML4\n");
        return;
      }
      pml4[pml4_idx] = new_pt_phys | 0x3;
      invlpg((void*) va);
    }
    uint64_t* pdpt_va = get_pdpt(va);
    if (!(pdpt_va[pdpt_idx] & 1))
    {
      serial_puts("[DEBUG] heap: new PDPT\n");
      uint64_t new_pt_phys = alloc_page();
      if (!new_pt_phys) {
        serial_puts("[DEBUG] heap: alloc_page failed for PDPT\n");
        return;
      }
      pdpt_va[pdpt_idx] = new_pt_phys | 0x3;
      invlpg((void*) va);
    }
    uint64_t* pd_va = get_pd(va);
    if (!(pd_va[pd_idx] & 1))
    {
      serial_puts("[DEBUG] heap: new PD\n");
      uint64_t new_pt_phys = alloc_page();
      if (!new_pt_phys) {
        serial_puts("[DEBUG] heap: alloc_page failed for PD\n");
        return;
      }
      pd_va[pd_idx] = new_pt_phys | 0x3;
      invlpg((void*) va);
    }
    uint64_t* pt_va = get_pt(va);
    pt_va[pt_idx]   = pa | 0x3;  // Present + Writable
    invlpg((void*) va);
    serial_puts("[DEBUG] heap: mapped\n");
  }
  serial_puts("[DEBUG] paging_identity_map_kernel_heap: end\n");
}

void paging_identity_map_kernel_sections(void)
{
  serial_puts("paging_identity_map_kernel_sections: start\n");

  extern uint64_t _text_start, _text_end;
  extern uint64_t _data_start, _data_end;
  extern uint64_t _rodata_start, _rodata_end;
  extern uint64_t _bss_start, _bss_end;
  uint64_t        vaddr =
      (uint64_t) &_text_start;  // any kernel section start will do for logging the mapping process
  uint64_t paddr = VIRT_TO_PHYS(vaddr);
  serial_puts("Kernel section virtual addresses:\n");
  serial_puts("  .text   ");
  serial_puthex64((uint64_t) &_text_start);
  serial_puts(" – ");
  serial_puthex64((uint64_t) &_text_end);
  serial_puts("\n");
  serial_puts("  .rodata ");
  serial_puthex64((uint64_t) &_rodata_start);
  serial_puts(" – ");
  serial_puthex64((uint64_t) &_rodata_end);
  serial_puts("\n");
  serial_puts("  .data   ");
  serial_puthex64((uint64_t) &_data_start);
  serial_puts(" – ");
  serial_puthex64((uint64_t) &_data_end);
  serial_puts("\n");
  serial_puts("  .bss    ");
  serial_puthex64((uint64_t) &_bss_start);
  serial_puts(" – ");
  serial_puthex64((uint64_t) &_bss_end);
  serial_puts("\n");
  serial_puts("About to map va=0x");
  serial_puthex64(vaddr);
  serial_puts(" → pa=0x");
  serial_puthex64(paddr);
  serial_puts("\n");
  if ((uint64_t) &_text_start == 0 || (uint64_t) &_text_end == 0 ||
      (uint64_t) &_rodata_start == 0 || (uint64_t) &_rodata_end == 0 ||
      (uint64_t) &_data_start == 0 || (uint64_t) &_data_end == 0 || (uint64_t) &_bss_start == 0 ||
      (uint64_t) &_bss_end == 0)
  {
    serial_puts("ERROR: kernel section symbols invalid!\n");
    for (;;)
      ;
  }
  serial_puts("paging_identity_map_kernel_sections: mapping kernel sections...\n");
  serial_puts("  _text_start: 0x"); serial_puthex64((uint64_t)&_text_start); serial_puts("\n");
  serial_puts("  _text_end:   0x"); serial_puthex64((uint64_t)&_text_end); serial_puts("\n");
  serial_puts("  _rodata_start: 0x"); serial_puthex64((uint64_t)&_rodata_start); serial_puts("\n");
  serial_puts("  _rodata_end:   0x"); serial_puthex64((uint64_t)&_rodata_end); serial_puts("\n");
  serial_puts("  _data_start: 0x"); serial_puthex64((uint64_t)&_data_start); serial_puts("\n");
  serial_puts("  _data_end:   0x"); serial_puthex64((uint64_t)&_data_end); serial_puts("\n");
  serial_puts("  _bss_start: 0x"); serial_puthex64((uint64_t)&_bss_start); serial_puts("\n");
  serial_puts("  _bss_end:   0x"); serial_puthex64((uint64_t)&_bss_end); serial_puts("\n");
  static const struct {
    const char* name;
    uint64_t    vstart, vend;
    uint64_t    pte_flags;  // full PTE value except address
  } sections[] = {
      {".text",
       (uint64_t) &_text_start,
       (uint64_t) &_text_end,
       0x0000000000000001ULL},  // P, no NX → executable
      {".rodata",
       (uint64_t) &_rodata_start,
       (uint64_t) &_rodata_end,
       0x8000000000000001ULL},  // P + NX
      {".data",
       (uint64_t) &_data_start,
       (uint64_t) &_data_end,
       0x8000000000000003ULL},                                                        // P + W + NX
      {".bss", (uint64_t) &_bss_start, (uint64_t) &_bss_end, 0x8000000000000003ULL},  // P + W + NX
  };
  for (int i = 0; i < 4; i++) {
    serial_puts("section["); serial_puthex64(i); serial_puts("]: name="); serial_puts(sections[i].name);
    serial_puts(" vstart=0x"); serial_puthex64(sections[i].vstart);
    serial_puts(" vend=0x"); serial_puthex64(sections[i].vend);
    serial_puts(" pte_flags=0x"); serial_puthex64(sections[i].pte_flags);
    serial_puts("\n");
  }
  serial_puts("paging_identity_map_kernel_sections: about to loop over sections\n");
  uint64_t* pml4 = get_pml4();

  for (size_t i = 0; i < sizeof(sections) / sizeof(sections[0]); i++)
  {
    uint64_t vaddr     = sections[i].vstart & ~0xFFFULL;
    uint64_t end       = (sections[i].vend + 0xFFFULL) & ~0xFFFULL;
    uint64_t pte_flags = sections[i].pte_flags;

    serial_puts("[DEBUG] Section loop i="); serial_puthex64(i); serial_puts(" name="); serial_puts(sections[i].name); serial_puts(" vaddr=0x"); serial_puthex64(vaddr); serial_puts(" end=0x"); serial_puthex64(end); serial_puts("\n");

    if (vaddr >= end) {
      serial_puts("[DEBUG] vaddr >= end, skipping section\n");
      continue;
    }

    serial_puts("Mapping section ");
    serial_puts(sections[i].name);
    serial_puts(" va 0x");
    serial_puthex64(vaddr);
    serial_puts(" - 0x");
    serial_puthex64(end);
    serial_puts(" flags 0x");
    serial_puthex64(pte_flags);
    serial_puts("\n");

    while (vaddr < end)
    {
      serial_puts("[DEBUG] Top of while: vaddr=0x"); serial_puthex64(vaddr); serial_puts(" end=0x"); serial_puthex64(end); serial_puts("\n");
      uint64_t paddr = vaddr - KERNEL_VIRT_BASE;  // Calculate physical address

      // PML4
      uint64_t pml4_idx = (vaddr >> 39) & 0x1FF;
      serial_puts("[DEBUG] PML4 idx="); serial_puthex64(pml4_idx); serial_puts(" entry=0x"); serial_puthex64(pml4[pml4_idx]); serial_puts("\n");
      if (!(pml4[pml4_idx] & 1))
      {
        serial_puts("[DEBUG] Allocating new PML4 page\n");
        uint64_t new_phys = alloc_page();
        serial_puts("[DEBUG] alloc_page returned 0x"); serial_puthex64(new_phys); serial_puts("\n");
        if (!new_phys)
          goto out_of_memory;
        void* new_virt = PHYS_TO_VIRT(new_phys);
        serial_puts("[DEBUG] PHYS_TO_VIRT(new_phys) = "); serial_puthex64((uint64_t)new_virt); serial_puts("\n");
        serial_puts("[DEBUG] memset about to run\n");
        memset(new_virt, 0, 0x1000);
        serial_puts("[DEBUG] memset done\n");
        pml4[pml4_idx] = new_phys | 0x3;  // P + W
        invlpg((void*) vaddr);            // safe broad invalidation
        serial_puts("[DEBUG] New PML4 entry set\n");
      }

      // PDPT
      uint64_t* pdpt     = get_pdpt(vaddr);
      uint64_t  pdpt_idx = (vaddr >> 30) & 0x1FF;
      serial_puts("[DEBUG] PDPT idx="); serial_puthex64(pdpt_idx); serial_puts(" entry=0x"); serial_puthex64(pdpt[pdpt_idx]); serial_puts("\n");
      if (!(pdpt[pdpt_idx] & 1))
      {
        serial_puts("[DEBUG] Allocating new PDPT page\n");
        uint64_t new_phys = alloc_page();
        serial_puts("[DEBUG] alloc_page returned 0x"); serial_puthex64(new_phys); serial_puts("\n");
        if (!new_phys)
          goto out_of_memory;
        memset(PHYS_TO_VIRT(new_phys), 0, 0x1000);
        pdpt[pdpt_idx] = new_phys | 0x3;
        invlpg((void*) vaddr);
        serial_puts("[DEBUG] New PDPT entry set\n");
      }

      // PD
      uint64_t* pd     = get_pd(vaddr);
      uint64_t  pd_idx = (vaddr >> 21) & 0x1FF;
      serial_puts("[DEBUG] PD idx="); serial_puthex64(pd_idx); serial_puts(" entry=0x"); serial_puthex64(pd[pd_idx]); serial_puts("\n");
      if (!(pd[pd_idx] & 1))
      {
        serial_puts("[DEBUG] Allocating new PD page\n");
        uint64_t new_phys = alloc_page();
        serial_puts("[DEBUG] alloc_page returned 0x"); serial_puthex64(new_phys); serial_puts("\n");
        if (!new_phys)
          goto out_of_memory;
        memset(PHYS_TO_VIRT(new_phys), 0, 0x1000);
        pd[pd_idx] = new_phys | 0x3;
        invlpg((void*) vaddr);
        serial_puts("[DEBUG] New PD entry set\n");
      }

      // PT – final mapping
      uint64_t* pt     = get_pt(vaddr);
      uint64_t  pt_idx = (vaddr >> 12) & 0x1FF;
      serial_puts("[DEBUG] PT idx="); serial_puthex64(pt_idx); serial_puts("\n");
      pt[pt_idx]       = paddr | pte_flags;
      invlpg((void*) vaddr);
      serial_puts("[DEBUG] Page mapped: vaddr=0x"); serial_puthex64(vaddr); serial_puts(" paddr=0x"); serial_puthex64(paddr); serial_puts(" flags=0x"); serial_puthex64(pte_flags); serial_puts("\n");

      vaddr += 0x1000;
    }

    serial_puts("  done: ");
    serial_puts(sections[i].name);
    serial_puts("\n");
  }

  serial_puts("paging_identity_map_kernel_sections: finished\n");
  return;

out_of_memory:
  serial_puts("ERROR: out of memory while mapping kernel sections\n");
  for (;;)
    ;
}