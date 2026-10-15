bits 16

%include "jasmdef.asm"

header:
.magic:
  dd 1380014410 ; magic
.version:
  dd 01
.varchname:
  db "varchc16x16X32"
  times 32 - ($ - header.varchname) db 0
.cfuncs:
  dd 0

boot:
  load    0, 64
  org     0

boot.realloc:
  load    0, 64
  load    1, boot
  sub     0, 1

main:

times 1024 - ($ - boot) db 0

_Vm.Threads.thread2:
  load    0, 64
  org     0
  load    1, _Vm.Threads.thread2.do
  load    2, _Vm.Threads.thread2
  sub     

.do:
  beep
  jmp 
