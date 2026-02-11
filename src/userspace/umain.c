#include <stdint.h>

static inline uint64_t u_syscall(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3)
{
  uint64_t ret;
  asm volatile("mov %1, %%rax\n\t"
               "mov %2, %%rdi\n\t"
               "mov %3, %%rsi\n\t"
               "mov %4, %%rdx\n\t"
               "int $0x80\n\t"
               "mov %%rax, %0\n\t"
               : "=r"(ret)
               : "r"(num), "r"(a1), "r"(a2), "r"(a3)
               : "rax", "rdi", "rsi", "rdx");
  return ret;
}

void user_main(void)
{
  const char* hello = "user: Hello from user mode!\n";
  u_syscall(1, 1, (uint64_t) hello, 29);

  /* simple echo loop */
  char buf[128];
  while (1)
  {
    u_syscall(1, 1, (uint64_t) "sh> ", 4);
    int n = (int) u_syscall(2, 0, (uint64_t) buf, sizeof(buf));
    if (n <= 0)
      continue;
    /* echo back */
    u_syscall(1, 1, (uint64_t) buf, n);
  }
}
