#include "mem/alloc.h"
#include "mem/paging.h"
#include "serial/serial.h"
#include <stddef.h>
#include <stdint.h>

extern void user_main(void);

/* enter_user_task: kernel task that prepares a user stack and irets into
 * user_main */
void enter_user_task(void *arg) {
  void (*entry)(void) = (void (*)(void))arg;
  serial_puts("enter_user: preparing user stack\n");

  /* allocate a user stack */
  void *ustack = kmalloc(16 * 1024);
  if (!ustack) {
    serial_puts("enter_user: kmalloc failed\n");
    return;
  }
  /* ensure pages for the user stack are marked user-accessible */
  if (paging_set_user(ustack, 16 * 1024) != 0) {
    serial_puts("enter_user: paging_set_user failed\n");
    /* continue anyway; this will likely fault when iret'ing to user */
  } else {
    serial_puts("enter_user: user stack mapped as user-accessible\n");
  }
  uint64_t user_sp = (uint64_t)ustack + 16 * 1024 - 8;

  /* prepare iret frame and iret to user code (ring3) */
  asm volatile("cli\n"
               "pushq $0x23\n" /* user SS selector */
               "pushq %0\n"    /* user RSP */
               "pushfq\n"
               "pushq $0x1b\n" /* user CS selector */
               "pushq %1\n"    /* user RIP */
               "iretq\n"
               :
               : "r"(user_sp), "r"(entry));

  /* should not return */
  for (;;)
    asm volatile("hlt");
}
