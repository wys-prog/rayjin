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

  cmp 0, 1
  load 3, greater
  add 3, 4
  sub 3, 5
  jg 3

end:
  halt

greater:
  beep
  jmp 3
  halt

times 512 halt

thread2:
