#include "syscall/syscall.h"
#include "console/console.h"
#include "drivers/keyboard/keyboard.h"
#include "serial/serial.h"
#include <stdint.h>
#include <string.h>
#include "mem/alloc.h"
#include "mem/paging.h"
#include "drivers/fat/fat.h"
#include "drivers/ext/ext.h"
#include "multitasking/scheduler.h"

/* Syscall numbers */
#define SYS_WRITE 1
#define SYS_READ 2
#define SYS_EXIT 3
#define SYS_EXEC 4

/* Minimal ELF structures for loader */
struct Elf64_Ehdr {
  unsigned char e_ident[16];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version;
  uint64_t e_entry;
  uint64_t e_phoff;
  uint64_t e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
};

struct Elf64_Phdr {
  uint32_t p_type;
  uint32_t p_flags;
  uint64_t p_offset;
  uint64_t p_vaddr;
  uint64_t p_paddr;
  uint64_t p_filesz;
  uint64_t p_memsz;
  uint64_t p_align;
};

/* Arg passed to the starter task */
struct elf_start_arg {
  void *entry;
  void *ustack;
};

/* starter: iret into loaded ELF using provided entry and user stack */
__attribute__((noreturn)) static void start_loaded_elf(void *arg) {
  struct elf_start_arg *a = (struct elf_start_arg *)arg;
  void (*entry)(void) = (void (*)(void))a->entry;
  void *ustack = a->ustack;
  serial_puts("enter_user: starting loaded ELF\n");
  uint64_t user_sp = (uint64_t)ustack + 16 * 1024 - 8;
  asm volatile("cli\n"
               "pushq $0x23\n" /* user SS */
               "pushq %0\n"
               "pushfq\n"
               "pushq $0x1b\n" /* user CS */
               "pushq %1\n"
               "iretq\n"
               :
               : "r"(user_sp), "r"(entry));
  for (;;)
    asm volatile("hlt");
}

/* try to read a file by path using ext then fat */
static int read_file_from_fs(const char *path, void **out_buf, size_t *out_len) {
  if (!path || !out_buf || !out_len) return -1;
  if (ext_read_file(path, out_buf, out_len) == 0) return 0;
  if (fat_read_file(path, out_buf, out_len) == 0) return 0;
  return -1;
}

/* spawn ELF from memory buffer: load PT_LOAD segments and create a user task */
static int spawn_elf_from_buf(void *buf, size_t len) {
  if (!buf || len < sizeof(struct Elf64_Ehdr)) return -1;
  struct Elf64_Ehdr *eh = (struct Elf64_Ehdr *)buf;
  if (eh->e_ident[0] != 0x7f || eh->e_ident[1] != 'E' || eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F') return -1;
  /* iterate program headers */
  struct Elf64_Phdr *ph = (struct Elf64_Phdr *)((uint8_t *)buf + eh->e_phoff);
  uint64_t min_vaddr = (uint64_t)-1, max_vaddr = 0;
  for (int i = 0; i < eh->e_phnum; ++i) {
    if (ph[i].p_type != 1) continue; /* PT_LOAD */
    if (ph[i].p_memsz == 0) continue;
    if (ph[i].p_vaddr < min_vaddr) min_vaddr = ph[i].p_vaddr;
    uint64_t end = ph[i].p_vaddr + ph[i].p_memsz;
    if (end > max_vaddr) max_vaddr = end;
  }
  if (min_vaddr == (uint64_t)-1) return -1;
  /* size and align to page */
  uint64_t total = (max_vaddr - min_vaddr + 0xFFF) & ~0xFFFULL;
  void *raw = kmalloc(total + 0x1000);
  if (!raw) return -1;
  uint8_t *base = (uint8_t *)(((uintptr_t)raw + 0xFFF) & ~0xFFFULL);
  /* zero target region */
  for (uint64_t i = 0; i < total; ++i) base[i] = 0;
  /* copy segments */
  for (int i = 0; i < eh->e_phnum; ++i) {
    if (ph[i].p_type != 1) continue;
    uint64_t off = ph[i].p_offset;
    uint64_t filesz = ph[i].p_filesz;
    uint64_t memsz = ph[i].p_memsz;
    uint64_t dest_off = ph[i].p_vaddr - min_vaddr;
    if (off + filesz > len) { kfree(raw); return -1; }
    memcpy(base + dest_off, (uint8_t *)buf + off, filesz);
    if (memsz > filesz) memset(base + dest_off + filesz, 0, memsz - filesz);
  }
  /* mark pages user-accessible */
  if (paging_set_user(base, total) != 0) {
    /* continue anyway */
    serial_puts("spawn_elf: paging_set_user failed\n");
  }
  /* allocate user stack */
  void *ustack = kmalloc(16 * 1024);
  if (!ustack) { kfree(raw); return -1; }
  if (paging_set_user(ustack, 16 * 1024) != 0) serial_puts("spawn_elf: stack paging_set_user failed\n");
  /* create starter arg */
  struct elf_start_arg *a = kmalloc(sizeof(*a));
  if (!a) { kfree(raw); return -1; }
  a->entry = (void *)((uintptr_t)base + (uintptr_t)(eh->e_entry - min_vaddr));
  a->ustack = ustack;
  /* create kernel task that will iret to user entry */
  extern int task_create(task_fn fn, void *arg);
  int tid = task_create(start_loaded_elf, a);
  if (tid < 0) { kfree(a); return -1; }
  return tid;
}

uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3) {
  (void)a3;
  if (num == SYS_WRITE) {
    int fd = (int)a1;
    const char *buf = (const char *)(uintptr_t)a2;
    int len = (int)a3;
    if (fd == 1) {
      /* write to console and serial */
      if (buf && len > 0) {
        /* make a small buffer copy */
        for (int i = 0; i < len; ++i) {
          char c = buf[i];
          char s[2] = {c, '\0'};
          serial_puts(s);
          /* print on framebuffer console */
          char t[2] = {c, '\0'};
          console_puts(t);
        }
      }
      return (uint64_t)len;
    }
    return (uint64_t)-1;
  } else if (num == SYS_READ) {
    int fd = (int)a1;
    char *buf = (char *)(uintptr_t)a2;
    int len = (int)a3;
    if (fd == 0) {
      int read = 0;
      while (read < len) {
        int c = keyboard_getchar();
        if (c == -1) {
          /* yield to allow other tasks */
          extern void scheduler_yield(void);
          scheduler_yield();
          continue;
        }
        buf[read++] = (char)c;
        if (c == '\n')
          break;
      }
      return (uint64_t)read;
    }
    return (uint64_t)-1;
  } else if (num == SYS_EXIT) {
    /* exit current task: mark as dead and yield */
    extern int scheduler_get_current(void);
    extern void scheduler_mark_dead(int id);
    int cur = scheduler_get_current();
    if (cur >= 0)
      scheduler_mark_dead(cur);
    return 0;
  }

  else if (num == SYS_EXEC) {
    const char *path = (const char *)(uintptr_t)a1;
    if (!path) return (uint64_t)-1;
    void *buf = NULL;
    size_t len = 0;
    if (read_file_from_fs(path, &buf, &len) != 0) {
      serial_puts("syscall: exec read_file failed\n");
      return (uint64_t)-1;
    }
    int tid = spawn_elf_from_buf(buf, len);
    /* free the file buffer if it was allocated by fs */
    if (buf) kfree(buf);
    if (tid < 0) {
      serial_puts("syscall: spawn_elf failed\n");
      return (uint64_t)-1;
    }
    return (uint64_t)tid;
  }

  serial_puts("syscall: unknown num\n");
  return (uint64_t)-1;
}
