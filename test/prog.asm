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
main:
  load 0, 12
  load 1, 3

  load 4, 64
  load 5, boot

  org 4
  panic 67

thread2:
