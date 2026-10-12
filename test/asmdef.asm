%define nop   db 0

%macro cuop 2
  db 1, %1, %2
%endmacro

%macro add 2
  db 2, %1, %2
%endmacro

%macro sub 2
  db 3, %1, %2
%endmacro

%macro mul 2
  db 4, %1, %2
%endmacro

%macro div 2
  db 5, %1, %2
%endmacro

%macro mod 2
  db 6, %1, %2
%endmacro

%macro smop 2
  db 7, %1, %2
%endmacro

%macro smadd 2
  db 8, %1, %2
%endmacro

%macro smsub 2
  db 9, %1, %2
%endmacro

%macro smmul 2
  db 10, %1, %2
%endmacro

%macro smdiv 2
  db 11, %1, %2
%endmacro

%macro smmod 2
  db 12, %1, %2
%endmacro

%macro ctrl 2
  db 13, %1, %2
%endmacro

%macro org 1
  db 14, %1
%endmacro

%macro jmp 1
  db 15, %1
%endmacro

%macro jz 1
  db 16, %1
%endmacro

%macro jnz 1
  db 17, %1
%endmacro

%macro jq 1
  db 18, %1
%endmacro

%macro jnq 1
  db 19, %1
%endmacro

%macro jg 1
  db 20, %1
%endmacro

%macro jl 1
  db 21, %1
%endmacro

%macro jgq 1
  db 22, %1
%endmacro

%macro jlq 1
  db 23, %1
%endmacro

%macro cmpcu 2
  db 24, %1, %2
%endmacro

%macro cmpsm 2
  db 25, %1, %2
%endmacro

%macro testcu 1
  db 26, %1
%endmacro

%macro testsm 1
  db 27, %1
%endmacro

%macro dtcu 1
  db 28, %1
%endmacro

%macro load 2
  db 29, %1, %2
%endmacro

%macro write 2
  db 30, %1, %2
%endmacro

%macro mov 2
  db 31, %1, %2
%endmacro

%macro movx 2
  db 32, %1, %2
%endmacro

%macro swap 2
  db 33, %1, %2
%endmacro

%macro dtsm 1
  db 34, %1
%endmacro

%macro smload 2
  db 35, %1, %2
%endmacro

%macro smwrite 2
  db 36, %1, %2
%endmacro

%macro smmov 2
  db 37, %1, %2
%endmacro

%macro smmovx 2
  db 38, %1, %2
%endmacro

%macro smswap 2
  db 39, %1, %2
%endmacro

%macro bool 2
  db 40, %1, %2
%endmacro

%macro or 2
  db 41, %1, %2
%endmacro

%macro xor 2
  db 42, %1, %2
%endmacro

%macro and 2
  db 43, %1, %2
%endmacro

%macro nand 2
  db 44, %1, %2
%endmacro

%macro not 1
  db 45, %1
%endmacro

%macro smbool 2
  db 46, %1, %2
%endmacro

%macro smor 2
  db 47, %1, %2
%endmacro

%macro smxor 2
  db 48, %1, %2
%endmacro

%macro smand 2
  db 49, %1, %2
%endmacro

%macro smnand 2
  db 50, %1, %2
%endmacro

%macro smnot 1
  db 51, %1
%endmacro

%macro shl 2
  db 52, %1, %2
%endmacro

%macro shr 2
  db 53, %1, %2
%endmacro

%macro smshl 2
  db 54, %1, %2
%endmacro

%macro smshr 2
  db 55, %1, %2
%endmacro

%macro sys 1
  db 56, %1
%endmacro

%define halt  db 57

%macro panic 1
  db 58, %1
%endmacro

%define beep  db 59