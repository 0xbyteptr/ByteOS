#include "paging.h"
#include "alloc.h"
#include "serial/serial.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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
  uint64_t cr3 = read_cr3();
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
  pml4_virt[PML4_RECURSIVE_INDEX] = pml4_phys | 0x3;  // Present + Writable
  invlpg((void*) PML4_RECURSIVE_VADDR);
  serial_puts("paging_init: recursive mapping set up\n");
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
  void* va = kmalloc(0x2000);
  if (!va)
  {
    serial_puts("alloc_page: kmalloc failed\n");
    return 0;
  }

  /* Align to page boundary */
  uintptr_t aligned_va = ((uintptr_t) va + 0xFFF) & ~0xFFFULL;
  memset((void*) aligned_va, 0, 0x1000);

  uint64_t phys = paging_get_phys(aligned_va);
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
      serial_puts("paging_map_user_va: no phys mapping for kernel VA ");
      serial_puthex64(src_va);
      serial_puts("\n");
      return -1;
    }

    uint64_t pml4_idx = (dst_va >> 39) & 0x1FF;
    uint64_t pdpt_idx = (dst_va >> 30) & 0x1FF;
    uint64_t pd_idx   = (dst_va >> 21) & 0x1FF;
    uint64_t pt_idx   = (dst_va >> 12) & 0x1FF;

    /* Create PML4 entry if missing */
    if (!(pml4[pml4_idx] & 1))
    {
      uint64_t new_pt_phys = alloc_page();
      if (!new_pt_phys)
        return -1;
      pml4[pml4_idx] = new_pt_phys | 0x7;  // P + W + U
      invlpg((void*) dst_va);
    }

    uint64_t* pdpt_va = get_pdpt(dst_va);
    if (!(pdpt_va[pdpt_idx] & 1))
    {
      uint64_t new_pt_phys = alloc_page();
      if (!new_pt_phys)
        return -1;
      pdpt_va[pdpt_idx] = new_pt_phys | 0x7;
      invlpg((void*) dst_va);
    }

    uint64_t* pd_va = get_pd(dst_va);
    if (!(pd_va[pd_idx] & 1))
    {
      uint64_t new_pt_phys = alloc_page();
      if (!new_pt_phys)
        return -1;
      pd_va[pd_idx] = new_pt_phys | 0x7;
      invlpg((void*) dst_va);
    }

    uint64_t* pt_va = get_pt(dst_va);
    pt_va[pt_idx]   = phys | 0x7;  // Present + Writable + User
    invlpg((void*) dst_va);

    serial_puts("Mapped user VA 0x");
    serial_puthex64(dst_va);
    serial_puts(" -> phys 0x");
    serial_puthex64(phys);
    serial_puts("\n");
  }

  return 0;
}

// Ensure kmalloc'd region is mapped in kernel page tables
void paging_map_kernel_va(uint64_t kernel_va, size_t size)
{
  uint64_t* pml4 = get_pml4();
  for (uint64_t offset = 0; offset < size; offset += 0x1000)
  {
    uint64_t va       = kernel_va + offset;
    uint64_t pml4_idx = (va >> 39) & 0x1FF;
    uint64_t pdpt_idx = (va >> 30) & 0x1FF;
    uint64_t pd_idx   = (va >> 21) & 0x1FF;
    uint64_t pt_idx   = (va >> 12) & 0x1FF;
    // Create PML4 entry if missing
    if (!(pml4[pml4_idx] & 1))
    {
      uint64_t new_pt_phys = alloc_page();
      if (!new_pt_phys)
        return;
      pml4[pml4_idx] = new_pt_phys | 0x3;  // P + W
      invlpg((void*) va);
    }
    uint64_t* pdpt_va = get_pdpt(va);
    if (!(pdpt_va[pdpt_idx] & 1))
    {
      uint64_t new_pt_phys = alloc_page();
      if (!new_pt_phys)
        return;
      pdpt_va[pdpt_idx] = new_pt_phys | 0x3;
      invlpg((void*) va);
    }
    uint64_t* pd_va = get_pd(va);
    if (!(pd_va[pd_idx] & 1))
    {
      uint64_t new_pt_phys = alloc_page();
      if (!new_pt_phys)
        return;
      pd_va[pd_idx] = new_pt_phys | 0x3;
      invlpg((void*) va);
    }
    uint64_t* pt_va = get_pt(va);
    uint64_t  phys  = VIRT_TO_PHYS(va);
    pt_va[pt_idx]   = phys | 0x3;  // Present + Writable
    invlpg((void*) va);
    // Debug output for each mapped page
    extern void serial_puts(const char*);
    extern void serial_puthex64(uint64_t);
    serial_puts("paging_map_kernel_va: mapped VA 0x");
    serial_puthex64(va);
    serial_puts(" -> PA 0x");
    serial_puthex64(phys);
    serial_puts("\n");
  }
}

void paging_identity_map_kernel_heap(void)
{
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
    if (!(pml4[pml4_idx] & 1))
    {
      uint64_t new_pt_phys = alloc_page();
      if (!new_pt_phys)
        return;
      pml4[pml4_idx] = new_pt_phys | 0x3;
      invlpg((void*) va);
    }
    uint64_t* pdpt_va = get_pdpt(va);
    if (!(pdpt_va[pdpt_idx] & 1))
    {
      uint64_t new_pt_phys = alloc_page();
      if (!new_pt_phys)
        return;
      pdpt_va[pdpt_idx] = new_pt_phys | 0x3;
      invlpg((void*) va);
    }
    uint64_t* pd_va = get_pd(va);
    if (!(pd_va[pd_idx] & 1))
    {
      uint64_t new_pt_phys = alloc_page();
      if (!new_pt_phys)
        return;
      pd_va[pd_idx] = new_pt_phys | 0x3;
      invlpg((void*) va);
    }
    uint64_t* pt_va = get_pt(va);
    pt_va[pt_idx]   = pa | 0x3;  // Present + Writable
    invlpg((void*) va);
    extern void serial_puts(const char*);
    extern void serial_puthex64(uint64_t);
    serial_puts("paging_identity_map_kernel_heap: VA 0x");
    serial_puthex64(va);
    serial_puts(" -> PA 0x");
    serial_puthex64(pa);
    serial_puts("\n");
  }
}

// Identity-map .text, .data, .rodata, .bss for kernel access
void paging_identity_map_kernel_sections(void)
{
  serial_puts("paging_identity_map_kernel_sections: start\n");
  extern uint64_t _text_start, _text_end;
  extern uint64_t _data_start, _data_end;
  extern uint64_t _rodata_start, _rodata_end;
  extern uint64_t _bss_start, _bss_end;
  serial_puts("Section symbol values:\n");
  serial_puts("_text_start=0x");
  serial_puthex64((uint64_t) &_text_start);
  serial_puts(" _text_end=0x");
  serial_puthex64((uint64_t) &_text_end);
  serial_puts("\n");
  serial_puts("_data_start=0x");
  serial_puthex64((uint64_t) &_data_start);
  serial_puts(" _data_end=0x");
  serial_puthex64((uint64_t) &_data_end);
  serial_puts("\n");
  serial_puts("_rodata_start=0x");
  serial_puthex64((uint64_t) &_rodata_start);
  serial_puts(" _rodata_end=0x");
  serial_puthex64((uint64_t) &_rodata_end);
  serial_puts("\n");
  serial_puts("_bss_start=0x");
  serial_puthex64((uint64_t) &_bss_start);
  serial_puts(" _bss_end=0x");
  serial_puthex64((uint64_t) &_bss_end);
  serial_puts("\n");
  struct
  {
    const char* name;
    uint64_t    start, end;
  } sections[] = {{".text", (uint64_t) &_text_start, (uint64_t) &_text_end},
                  {".data", (uint64_t) &_data_start, (uint64_t) &_data_end},
                  {".rodata", (uint64_t) &_rodata_start, (uint64_t) &_rodata_end},
                  {".bss", (uint64_t) &_bss_start, (uint64_t) &_bss_end}};
  for (int s = 0; s < 4; ++s)
  {
    if (sections[s].start >= sections[s].end)
    {
      serial_puts("Warning: section ");
      serial_puts(sections[s].name);
      serial_puts(" has invalid range, skipping\n");
      continue;
    }
    serial_puts("Mapping section: ");
    serial_puts(sections[s].name);
    serial_puts(" start=0x");
    serial_puthex64(sections[s].start);
    serial_puts(" end=0x");
    serial_puthex64(sections[s].end);
    serial_puts("\n");
    uint64_t max_map = 16 * 1024 * 1024;  // 16 MiB safety limit
    uint64_t map_end = sections[s].start + max_map;
    if (sections[s].end < map_end)
      map_end = sections[s].end;
    for (uint64_t va = sections[s].start; va < map_end; va += 0x1000)
    {
      uint64_t  pa       = va - 0xFFFFFFFF80000000ULL;
      uint64_t* pml4     = get_pml4();
      uint64_t  pml4_idx = (va >> 39) & 0x1FF;
      uint64_t  pdpt_idx = (va >> 30) & 0x1FF;
      uint64_t  pd_idx   = (va >> 21) & 0x1FF;
      uint64_t  pt_idx   = (va >> 12) & 0x1FF;
      if (!(pml4[pml4_idx] & 1))
      {
        uint64_t new_pt_phys = alloc_page();
        if (!new_pt_phys)
          return;
        pml4[pml4_idx] = new_pt_phys | 0x3;
        invlpg((void*) va);
      }
      uint64_t* pdpt_va = get_pdpt(va);
      if (!(pdpt_va[pdpt_idx] & 1))
      {
        uint64_t new_pt_phys = alloc_page();
        if (!new_pt_phys)
          return;
        pdpt_va[pdpt_idx] = new_pt_phys | 0x3;
        invlpg((void*) va);
      }
      uint64_t* pd_va = get_pd(va);
      if (!(pd_va[pd_idx] & 1))
      {
        uint64_t new_pt_phys = alloc_page();
        if (!new_pt_phys)
          return;
        pd_va[pd_idx] = new_pt_phys | 0x3;
        invlpg((void*) va);
      }
      uint64_t* pt_va = get_pt(va);
      pt_va[pt_idx]   = pa | 0x3;
      invlpg((void*) va);
    }
    serial_puts("Section mapped: ");
    serial_puts(sections[s].name);
    serial_puts("\n");
  }
  serial_puts("paging_identity_map_kernel_sections: done\n");
}