%include "asmdef.asm"

header:
.magic:
  dd 1380014410 ; magic
.version:
  dd 01
.varchname:
  db "varchc16x16X32"
  times 32 - ($ - header.varchname) db 0
.cfuncs:


times 16 db 0

add 1, 2

db 241
times 512 db 243

; todo: code + default arch (C++) + NASM?