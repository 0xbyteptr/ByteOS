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

// Embedded ByteBox binary
extern unsigned char _binary_bin_assets_bytebox_tmp_start[];
extern unsigned char _binary_bin_assets_bytebox_tmp_end[];

#include "multitasking/scheduler.h"

/* Syscall numbers */
#define SYS_READ  0
#define SYS_WRITE 1
#define SYS_OPEN  2
#define SYS_CLOSE 3
#define SYS_STAT  4
#define SYS_FSTAT 5
#define SYS_LSEEK 8
#define SYS_MMAP  9
#define SYS_MUNMAP 11
#define SYS_BRK   12
#define SYS_IOCTL 16
#define SYS_GETPID 39
#define SYS_GETPPID 110
#define SYS_GETUID 102
#define SYS_GETEUID 107
#define SYS_GETGID 104
#define SYS_GETEGID 108
#define SYS_UNAME 63
#define SYS_ARCH_PRCTL 158
#define SYS_SET_TID_ADDRESS 218
#define SYS_EXIT_GROUP 231
#define SYS_EXIT 60
#define SYS_EXEC 59

/* Minimal ELF64 structures */
struct Elf64_Ehdr {
    unsigned char e_ident[16];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint64_t      e_entry;
    uint64_t      e_phoff;
    uint64_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
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

/* Argument passed to the starter task */
struct elf_start_arg {
    void *entry;
    void *ustack;
};

/* Task that iretqs into user mode */
__attribute__((noreturn)) static void start_loaded_elf(void *arg) {
    struct elf_start_arg *a = arg;
    void (*entry)(void) = a->entry;
    void *ustack = a->ustack;

    serial_puts("start_loaded_elf: entering user mode\n");
    serial_puts("  entry = "); serial_puthex64((uint64_t)(uintptr_t)entry);
    serial_puts("\n");

    /* Stack grows down — start from the top and align properly */
    uint64_t user_sp = (uint64_t)ustack + 16 * 1024;
    user_sp &= ~0xFULL;           // 16-byte alignment (System V ABI)
    user_sp -= 8;                 // extra safety for return address alignment

    serial_puts("  rsp   = "); serial_puthex64(user_sp);
    serial_puts("\n");

    asm volatile(
        "cli\n"
        "pushq $0x23\n"           /* user SS (data segment) */
        "pushq %0\n"              /* user RSP */
        "pushfq\n"
        "popq %%rax\n"
        "orq  $0x200, %%rax\n"    /* set IF = 1 (enable interrupts in user mode) */
        "pushq %%rax\n"
        "pushq $0x1b\n"           /* user CS (code segment) */
        "pushq %1\n"              /* entry RIP */
        "iretq"
        :
        : "r"(user_sp), "r"((uint64_t)entry)
        : "rax", "memory"
    );

    __builtin_unreachable();
}

static int read_file_from_fs(const char *path, void **out_buf, size_t *out_len, int *is_embedded) {
    if (!path || !out_buf || !out_len || !is_embedded) return -1;

    *out_buf = NULL;
    *out_len = 0;
    *is_embedded = 0;

    unsigned char *bb_start = _binary_bin_assets_bytebox_tmp_start;
    unsigned char *bb_end   = _binary_bin_assets_bytebox_tmp_end;

    if (bb_start && (strcmp(path, "/bin/sh") == 0 || strcmp(path, "/bin/bytebox") == 0)) {
        serial_puts("read_file_from_fs: using embedded ByteBox for ");
        serial_puts(path);
        serial_puts("\n");

        size_t sz = (size_t)(bb_end - bb_start);
        *out_buf = (void *)bb_start;  // use directly, no copy
        *out_len = sz;
        *is_embedded = 1;   // mark as embedded, don't free
        return 0;
    }

    if (ext_read_file(path, out_buf, out_len) == 0) {
        serial_puts("read_file_from_fs: loaded via ext\n");
        return 0;
    }

    if (fat_read_file(path, out_buf, out_len) == 0) {
        serial_puts("read_file_from_fs: loaded via fat\n");
        return 0;
    }

    serial_puts("read_file_from_fs: failed to find file: ");
    serial_puts(path);
    serial_puts("\n");
    return -1;
}

/* Spawn user task from ELF buffer */
int spawn_elf_from_buf(void *buf, size_t len) {
    if (!buf || len < sizeof(struct Elf64_Ehdr)) {
        serial_puts("spawn_elf_from_buf: invalid buffer or too small\n");
        return -1;
    }

    struct Elf64_Ehdr *eh = buf;

    /* Very basic ELF validation */
    if (eh->e_ident[0] != 0x7f || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L'  || eh->e_ident[3] != 'F' ||
        eh->e_ident[4] != 2 /* 64-bit */) {
        serial_puts("spawn_elf_from_buf: not a valid 64-bit ELF\n");
        return -1;
    }

    serial_puts("spawn_elf_from_buf: ELF valid\n");

    if (eh->e_phoff == 0 || eh->e_phnum == 0) {
        serial_puts("spawn_elf_from_buf: no program headers\n");
        return -1;
    }

    serial_puts("spawn_elf_from_buf: loading segments\n");

    struct Elf64_Phdr *ph = (void *)((uint8_t *)buf + eh->e_phoff);
    serial_puts("spawn_elf_from_buf: program header offset = ");
    serial_puthex64(eh->e_phoff);
    serial_puts(", number of headers = ");
    serial_putdec(eh->e_phnum);
    serial_puts("\n");
    /* Compute address range of PT_LOAD segments */
    uint64_t min_vaddr = -1ULL;
    uint64_t max_vaddr = 0;
    serial_puts("spawn_elf_from_buf: scanning program headers for PT_LOAD segments\n");
    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != 1 /* PT_LOAD */) continue;
        if (ph[i].p_memsz == 0) continue;

        if (ph[i].p_vaddr < min_vaddr) min_vaddr = ph[i].p_vaddr;
        uint64_t end = ph[i].p_vaddr + ph[i].p_memsz;
        if (end > max_vaddr) max_vaddr = end;
    }
    serial_puts("spawn_elf_from_buf: min_vaddr = ");
    serial_puthex64(min_vaddr);
    serial_puts(", max_vaddr = ");
    serial_puthex64(max_vaddr);
    if (min_vaddr == -1ULL) {
        serial_puts("spawn_elf_from_buf: no PT_LOAD segments found\n");
        return -1;
    }
    serial_puts("spawn_elf_from_buf: before paging_map_user_va\n");
    const uint64_t stack_va = 0x7FFFFFFF0000ULL;
    uint64_t total_size = (max_vaddr - min_vaddr + 0xFFF) & ~0xFFFULL;
    serial_puts("spawn_elf_from_buf: total size to allocate = ");
    serial_puthex64(total_size);
    serial_puts("\n");
    /* Allocate aligned memory for the image */
    void *raw = kmalloc(total_size + 0x1000);
    if (!raw) {
        serial_puts("spawn_elf_from_buf: kmalloc for image failed\n");
        return -1;
    }
    serial_puts("spawn_elf_from_buf: kmalloc for image succeeded, raw = ");
    serial_puthex64((uint64_t)(uintptr_t)raw);
    serial_puts("\n");
    uint8_t *base = (uint8_t *)(((uintptr_t)raw + 0xFFF) & ~0xFFFULL);
    serial_puts("spawn_elf_from_buf: aligned base = ");
    serial_puthex64((uint64_t)(uintptr_t)base);
    serial_puts("\n");
    /* Zero the region */
    serial_puts("spawn_elf_from_buf: about to memset ELF image\n");
    // Only rely on bootloader mapping for kmalloc'd heap
    serial_puts("spawn_elf_from_buf: skipping kernel region mapping, relying on bootloader\n");
    memset(base, 0, total_size);
    serial_puts("spawn_elf_from_buf: memset done\n");
    serial_puts("spawn_elf_from_buf: image allocated at ");
    serial_puthex64((uint64_t)base);
    serial_puts("\n");
    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != 1) continue;

        uint64_t dest_off = ph[i].p_vaddr - min_vaddr;

        if (ph[i].p_offset + ph[i].p_filesz > len) {
            serial_puts("spawn_elf_from_buf: segment exceeds file size\n");
            kfree(raw);
            return -1;
        }

        memcpy(base + dest_off, (uint8_t *)buf + ph[i].p_offset, ph[i].p_filesz);

        if (ph[i].p_memsz > ph[i].p_filesz) {
            memset(base + dest_off + ph[i].p_filesz, 0,
                   ph[i].p_memsz - ph[i].p_filesz);
        }
    }

    serial_puts("spawn_elf_from_buf: segments loaded\n");

    /* Allocate user stack */
    void *ustack = kmalloc(16 * 1024);
    if (!ustack) {
        serial_puts("spawn_elf_from_buf: kmalloc ustack failed\n");
        kfree(raw);
        return -1;
    }
    memset(ustack, 0, 16 * 1024);
    if (paging_map_user_va(stack_va, (uint64_t)ustack, 16 * 1024) != 0) {
        serial_puts("spawn_elf_from_buf: paging_map_user_va failed for stack\n");
        kfree(ustack);
        kfree(raw);
        return -1;
    }
    serial_puts("spawn_elf_from_buf: stack done\n");
    /* Prepare argument for starter task */
    struct elf_start_arg *arg = kmalloc(sizeof(*arg));
    if (!arg) {
        serial_puts("spawn_elf_from_buf: kmalloc arg failed\n");
        kfree(ustack);
        kfree(raw);
        return -1;
    }

    arg->entry = (void *)eh->e_entry;
    arg->ustack = (void *)stack_va;

    /* Create the task */
    extern int task_create(task_fn fn, void *arg);
    int tid = task_create(start_loaded_elf, arg);

    if (tid < 0) {
        serial_puts("spawn_elf_from_buf: task_create failed\n");
        kfree(arg);
        kfree(ustack);
        kfree(raw);
        return -1;
    }

    serial_puts("spawn_elf_from_buf: task created\n");

    serial_puts("spawn_elf_from_buf: created task tid=");
    serial_putdec((uint64_t)tid);
    serial_puts("\n");

    /* Image memory is now owned by the user task — do not free here */
    return tid;
}

/* Kernel helper: spawn ELF by path */
int kernel_spawn_elf_from_path(const char *path) {
    if (!path) return -1;

    void *buf = NULL;
    size_t len = 0;
    int is_embedded = 0;

    serial_puts("kernel: attempting to spawn ");
    serial_puts(path);
    serial_puts("...\n");

    if (read_file_from_fs(path, &buf, &len, &is_embedded) != 0) {
        serial_puts("kernel_spawn_elf_from_path: failed to read file\n");
        return -1;
    }

    int tid = spawn_elf_from_buf(buf, len);

    /* Only free buffer if we allocated a copy */
    if (buf && !is_embedded) {
        kfree(buf);
    }

    if (tid < 0) {
        serial_puts("kernel_spawn_elf_from_path: spawn failed\n");
    }

    return tid;
}

/* Page fault handler */
void page_fault_handler(uint64_t addr) {
    serial_puts("Page fault\n");
    // For now, halt
    for (;;) asm volatile("hlt");
}

/* Syscall dispatcher */
uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3) {
    serial_puts("syscall: ");
    serial_putdec(num);
    serial_puts("\n");

    switch (num) {
        case SYS_WRITE: {
            int fd = (int)a1;
            const char *buf = (const char *)(uintptr_t)a2;
            int len = (int)a3;

            if (fd != 1 || !buf || len <= 0) {
                return -1ULL;
            }

            for (int i = 0; i < len; i++) {
                char c = buf[i];
                char s[2] = {c, '\0'};
                serial_puts(s);
                console_puts(s);
            }
            return (uint64_t)len;
        }

        case SYS_READ: {
            int fd = (int)a1;
            char *buf = (char *)(uintptr_t)a2;
            int len = (int)a3;

            if (fd != 0 || !buf || len <= 0) {
                return -1ULL;
            }

            int n = 0;
            while (n < len) {
                int c = keyboard_getchar();
                if (c == -1) {
                    extern void scheduler_yield(void);
                    scheduler_yield();
                    continue;
                }
                buf[n++] = (char)c;
                if (c == '\n') break;
            }
            return (uint64_t)n;
        }

        case SYS_EXIT: {
            extern int scheduler_get_current(void);
            extern void scheduler_mark_dead(int id);
            int cur = scheduler_get_current();
            if (cur >= 0) {
                scheduler_mark_dead(cur);
            }
            return 0;
        }

        case SYS_EXEC: {
            const char *path = (const char *)(uintptr_t)a1;
            if (!path) return -1ULL;

            void *buf = NULL;
            size_t len = 0;
            int is_embedded = 0;

            if (read_file_from_fs(path, &buf, &len, &is_embedded) != 0) {
                return -1ULL;
            }

            int tid = spawn_elf_from_buf(buf, len);

            if (buf && !is_embedded) {
                kfree(buf);
            }

            return tid < 0 ? -1ULL : (uint64_t)tid;
        }

        case SYS_OPEN: return -1ULL;
        case SYS_CLOSE: return 0;
        case SYS_BRK: {
            static uint64_t brk_addr = 0x600000;
            if (a1 == 0) return brk_addr;
            brk_addr = a1;
            return brk_addr;
        }
        case SYS_MMAP: return -1ULL;
        case SYS_MUNMAP: return 0;
        case SYS_FSTAT: return -1ULL;
        case SYS_STAT: return -1ULL;
        case SYS_LSEEK: return -1ULL;
        case SYS_GETPID: return 1;
        case SYS_GETPPID: return 1;
        case SYS_GETUID: return 0;
        case SYS_GETEUID: return 0;
        case SYS_GETGID: return 0;
        case SYS_GETEGID: return 0;
        case SYS_IOCTL: return -1ULL;
        case SYS_UNAME: return -1ULL;
        case SYS_ARCH_PRCTL: return 0;
        case SYS_SET_TID_ADDRESS: return 1;
        case SYS_EXIT_GROUP: {
            extern int scheduler_get_current(void);
            extern void scheduler_mark_dead(int id);
            int cur = scheduler_get_current();
            if (cur >= 0) {
                scheduler_mark_dead(cur);
            }
            return 0;
        }

        default:
            serial_puts("syscall: unknown number ");
            serial_putdec(num);
            serial_puts("\n");
            return -1ULL;
    }
}