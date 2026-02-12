// syscall.c
#include "syscall.h"
#include "serial/serial.h"     // assuming you have serial output
#include "console/console.h"   // optional: graphical console
#include "utils/log.h"         // optional: kernel logging

#include <stdint.h>
#include <stddef.h>

// ────────────────────────────────────────────────
// Syscall numbers (you can expand this list)
// ────────────────────────────────────────────────

#define SYS_EXIT      0
#define SYS_WRITE     1
#define SYS_READ      2
#define SYS_PUTS      3        // simple kernel puts (debug)
#define SYS_GETPID    4
#define SYS_YIELD     5
#define SYS_SLEEP     6
#define SYS_MMAP      10
#define SYS_MUNMAP    11
#define SYS_OPEN      20
#define SYS_CLOSE     21
#define SYS_READDIR   22

// Current process ID (dummy – replace with real scheduler later)
static uint64_t current_pid = 1;

// ────────────────────────────────────────────────
// Syscall handler – called from assembly interrupt handler
// Arguments come in registers (System V AMD64 ABI style):
//   num  = rdi
//   a1   = rsi
//   a2   = rdx
//   a3   = rcx   (sometimes used, but often rdi/rsi/rdx/r10)
// ────────────────────────────────────────────────

uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3)
{
    // Optional: log syscall entry (very useful for debugging)
    // log("syscall: num=%llu arg1=0x%llx arg2=0x%llx arg3=0x%llx", num, a1, a2, a3);

    switch (num)
    {
        case SYS_EXIT:
        {
            // void exit(int status)
            int status = (int)a1;
            serial_puts("syscall exit called with status ");
            serial_putdec(status);
            serial_puts("\n");

            // In real kernel: terminate current task, schedule next
            // For now: just halt (or panic in debug builds)
            serial_puts("Kernel: process exited, halting.\n");
            while (1) asm volatile("hlt");
            return 0; // unreachable
        }

        case SYS_WRITE:
        {
            // ssize_t write(int fd, const void *buf, size_t count)
            int    fd    = (int)a1;
            char*  buf   = (char*)a2;
            size_t count = (size_t)a3;

            if (fd == 1 || fd == 2) // stdout / stderr
            {
                size_t i;
                for (i = 0; i < count; i++)
                {
                    if (buf[i] == '\0') break;
                    serial_putc(buf[i]);
                    // console_putchar(buf[i]); // if you have graphical console
                }
                return i; // number of bytes written
            }
            else
            {
                serial_puts("syscall write: unsupported fd ");
                serial_putdec(fd);
                serial_puts("\n");
                return -1; // EBADF
            }
        }

        case SYS_READ:
        {
            // ssize_t read(int fd, void *buf, size_t count)
            // For simplicity: no real input yet
            serial_puts("syscall read: not implemented\n");
            return -1; // ENOSYS or EAGAIN
        }

        case SYS_PUTS:
        {
            // void puts(const char *s) – debug helper
            const char *s = (const char*)a1;
            if (s)
            {
                while (*s)
                {
                    serial_putc(*s++);
                }
                serial_putc('\n');
            }
            return 0;
        }

        case SYS_GETPID:
        {
            // pid_t getpid(void)
            return current_pid;
        }

        case SYS_YIELD:
        {
            // void yield(void) – give up CPU
            // In real kernel: call scheduler_yield()
            serial_puts("syscall yield called\n");
            // asm volatile("int $0x20"); // example software interrupt to scheduler
            return 0;
        }

        case SYS_SLEEP:
        {
            // unsigned int sleep(unsigned int seconds)
            uint64_t seconds = a1;
            serial_puts("syscall sleep(");
            serial_putdec(seconds);
            serial_puts(" seconds) – not implemented\n");
            return seconds; // pretend we slept
        }

        default:
        {
            serial_puts("unknown syscall: ");
            serial_puthex64(num);
            serial_puts("\n");
            return -1; // ENOSYS
        }
    }

    // Should never reach here
    return -1;
}