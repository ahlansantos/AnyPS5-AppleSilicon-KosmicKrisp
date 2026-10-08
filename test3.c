int foo() { return 0; }
__asm__(".globl _bar\n.set _bar, _foo");
