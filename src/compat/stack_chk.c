/* Minimal __stack_chk_fail implementation for freestanding kernel builds */

void __stack_chk_fail(void)
{
  /* Simple halt - stack protector violation should not occur in this toy
     kernel. Use an infinite loop so the linker resolves the symbol. */
  for (;;)
  {
  }
}
