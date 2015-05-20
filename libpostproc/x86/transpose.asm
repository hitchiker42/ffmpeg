;******************************************************************************
;*
;* Copyright (c) 2015 Tucker DiNapoli
;*
;* Functions to transpose 8x8 byte matrices
;*
;* This file is part of FFmpeg.
;*
;* FFmpeg is free software; you can redistribute it and/or modify
;* it under the terms of the GNU General Public License as published by
;* the Free Software Foundation; either version 2 of the License, or
;* (at your option) any later version.
;*
;* FFmpeg is distributed in the hope that it will be useful,
;* but WITHOUT ANY WARRANTY; without even the implied warranty of
;* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;* GNU General Public License for more details.
;*
;* You should have received a copy of the GNU General Public License
;* along with FFmpeg; if not, write to the Free Software
;* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
;*
%include "PPUtil.asm"

;;transpose blocks stored linewise in m0-m7
;;transposed blocks will also be stored linewise in m0-m7, but
;;this is done via renaming registers, so keep that in mind
;;For x86_64 using sse/avx we have 16 simd registers, so we can
;;avoid using the stack at all, for x86_32, or if we're using
;;mmx we only have 8 registers, so we need to store intermediate
;;results on the stack
%macro transpose_core_64 0
    mova m8, m0 ;1
    mova m9, m2 ;3
    mova m10, m4 ;5
    mova m11, m6 ;7

;; bytes->words
    punpcklbw m0, m1 ;14,04,13,03,12,02,11,01 = 0,1 L
    punpckhbw m8, m1 ;18,08,17,07,16,06,15,05 = 0,1 H
    mova m1, m0
    mova m12, m8

    punpcklbw m2, m3 ;2,3 L
    punpckhbw m9, m3 ;2,3 H

    punpcklbw m4, m5 ;4,5 L
    punpckhbw m10, m5 ;4,5 L
    mova m5, m4
    mova m13, m10

    punpcklbw m6, m7 ;6,7 L
    punpckhbw m11, m7 ;6,7 H

;; words->doublewords

    punpcklwd m0, m2 ;32,22,12,02,31,21,11,01 = 0,1 L4
    punpckhwd m1, m2 ;34,24,14,04,33,23,22,21 = 2,3 L4
    mova m2, m0
    mova m3, m1

    punpcklwd m4, m6 ;72,62,52,42,71,61,51,41 = 0,1 H4
    punpckhwd m5, m6 ;74,64,54,44,73,63,53,53 = 2,3 H4

    punpcklwd m8, m9 ;4,5 L4
    punpckhwd m12, m9 ;6,7 L4
    mova m6, m8
    mova m7, m12

    punpcklwd m10, m11 ;5,6 H4
    punpckhwd m13, m11 ;7,8 H4

;; doublewords->quadwords

    punpckldq m0, m4 ;71,61,51,41,31,21,11,01 = 0T
    punpckhdq m2, m4 ;72,62,52,42,32,22,12,02 = 1T

    punpckldq m1, m5 ;2T
    punpckhdq m3, m5 ;3T

    punpckldq m6, m10 ;4T
    punpckhdq m8, m10 ;5T

    punpckldq m7, m13 ;6T
    punpckldq m12, m13 ;7T
;;rename registers
    SWAP m1,m2
    SWAP m4,m6
    SWAP m5,m8
    SWAP m6,m7
    SWAP m7,m12
%endmacro
;;called with the first 4 rows of the block(s) to be transposed in m0-m3
;;and passed the locations of the last 4 rows as arguments. Uses 4*mmsize stack space
%macro transpose_core_32 4
    mova m4, m0 ;1
    mova m5, m2 ;3

;; ij is the j'th element of the i'th row
;; rows 0-3
;; bytes->words
    punpcklbw m0, m1 ;14,04,13,03,12,02,11,01 = 0,1 L
    punpckhbw m4, m1 ;18,08,17,07,16,06,15,05 = 0,1 H

    punpcklbw m2, m3 ;3,4 L
    punpckhbw m5, m3 ;3,4 H

;; we could use m6,m7, and just keep the results there
    mova m1, m0
    mova m3 ,m4
;; words->doublewords
    punpcklwd m0, m2 ;32,22,12,02,31,21,11,01
    punpckhwd m1, m2 ;34,24,14,04,33,23,13,03

    punpcklwd m3, m5 ;36,26,16,06,35,25,15,05
    punpckhwd m4, m5 ;38,28,18,08,37,27,17,07

;; store rows 0-3
    mova [rsp], m0
    mova [rsp + mmsize], m1
    mova [rsp + 2*mmsize], m3
    mova [rsp + 3*mmsize], m4

;; rows 4-7
    mova m0, %1 ;5
    mova m1, %2 ;6
    mova m2, %3 ;7
    mova m3, %4 ;8

    mova m4, m0
    mova m5, m2
;; bytes->words
    punpcklbw m0, m1 ;54,44,53,43,52,42,51,41 = 4,5 L
    punpckhbw m4, m1 ;58,48,57,47,56,46,55,45 = 4,5 H

    punpcklbw m2, m3 ;6,7 L
    punpckhbw m5, m3 ;6,7 H

    mova m1, m0
    mova m3, m2
;; words->doublewords
    punpcklwd m0, m2 ;72,62,52,42,71,61,51,41
    punpckhwd m1, m2 ;74,64,54,44,73,63,53,43

    punpcklwd m3, m5 ;76,66,56,46,75,65,55,45
    punpckhwd m4, m5 ;78,68,58,48,77,67,57,47

;; all rows, doublewords->quadwords
;; m2,m5,m6,m7 free
    mova m2, [rsp]
    mova m5, m2
    mova m6, [rsp + mmsize]
    mova m7, m6

    punpckldq m2, m0 ;0T
    punpckhdq m5, m0 ;1T
    punpckldq m6, m1 ;2T
    punpckhdq m7, m1 ;3T

;;store lines 2,3 again
    mova [rsp], m6
    mova [rsp + mmsize], m7
;;m0,m1,m6,m7 free
    mova m0, [rsp + 2*mmsize]
    mova m1, m0
    mova m6, [rsp + 3*mmsize]
    mova m7, m6

    punpckldq m0, m3 ;4T
    punpckhdq m1, m3 ;5T
    punpckldq m6, m4 ;6T
    punpckhdq m7, m4 ;7T

;;load lines 2,3
    mova m4, [rsp]
    mova m3, [rsp + mmsize]

;;rename registers
;;currently: m0=L4,m1=L5,m2=L0,m3=L3,m4=L2,m5=L1,m6=L6,m7=L7
    SWAP m0,m2 ;;m0=L0, m2=L4
    SWAP m2,m4 ;;m2=L2, m4=L4
    SWAP m1,m5
%endmacro


%macro gen_transpose 0
%if num_mmregs == 16
cglobal transpose, 4, 6, 16 ;src,src-stride, dst,dst-stride
    lea r4, [r0 + r1]
    lea r5, [r4 + r1]
    mova m0, [r0] ;1
    mova m1, [r4] ;2
    mova m2, [r0 + 2*r1] ;3
    mova m3, [r4 + 2*r1] ;4
    mova m4, [r0 + 4*r1] ;5
    mova m5, [r5] ;6
    mova m6, [r5 + r1] ;7
    mova m7, [r5 + 2*r1] ;8

    transpose_core_64

    lea r4, [r2 + r3]
    lea r5, [r4 + r3]

    mova [r2], m0
    mova [r2 + r3], m1
    mova [r4 + r2], m2
    mova [r4 + 2*r2], m3
    mova [r2 + 4*r3], m4
    mova [r5], m4
    mova [r5 + r3], m6
    mova [r5 + 2*r3], m7
    RET
%else ;;ARCH_x86_32 || mmx
cglobal transpose, 4, 6, 8, 4*mmsize ;src,src-stride, dst,dst-stride
    lea r4, [r0 + r1]
    lea r5, [r4 + r1]
    mova m0, [r0] ;1
    mova m1, [r4] ;2
    mova m2, [r0 + 2*r1] ;3
    mova m3, [r4 + 2*r1] ;4

    transpose_core_32 [r0+4*r1],[r4+4*r1],[r5+r1],[r5+2*r1]

    lea r4, [r2 + r3]
    lea r5, [r4 + r3]

    mova [r2], m0
    mova [r4], m1
    mova [r2 + 2*r3], m2
    mova [r4 + 2*r3], m3
    mova [r2 + 4*r3], m4
    mova [r4 + 4*r3], m5
    mova [r5 + r3], m6
    mova [r5 + 2*r3], m7
    RET
%endif
%endmacro

;; transpose and store 5 blocks into dst as follows:
;; |0|1|2|3|
;; |1|2|3|4|
;; this allows 2 consuctive blocks to be accessed, which
;; is necessary for some functions
;; if block 4 can't be accessed use a dummy block filled
;; with the last line of block 3

%macro gen_transpose_shift 0
cglobal transpose_shift, 3, 6, num_mmregs ;;src, srcStride, dst
    mov r3, 32 ;;dstStride
%if cpuflags(avx2)
;;transpose first 4 blocks of src
    call transpose
;; I think this is probably the best way to do this, but maybe not
    mov r4, 256
%assign %%i 0
%rep 8
    vextracti128 [r2 + r4 + (8 + 32*%%i)], m%%i, 1 ;;blocks 2,3 to second row of dst
    vpextrq [r2 + r4 + 32*%%i], m%%i, 1 ;;block 1 to second row of dst
%assign %%i %%i+1
%endrep
;; deal with extra block, use mmx transpose?
%elif cpuflags(sse2)
    call transpose
    mov r4, 256
%assign %%i 0
%rep 8
    pextrq [r2 + r5 + 32*%%i], m%%i, 1
%assign %%i %%i+1
%endrep
;;just to be safe
    RESET_MM_PERMUTATION
;;transpose next 2 blocks
    add r2, 16
    lea r0, [r0 + r1 * 8]
    call transpose
    mov r4, 256
%assign %%i 0
%rep 8
    movu [r2 + r4 + (32*%%i - 8)], m%%i
%assign %%i %%i+1
%endrep
%else ;;mmx
    call transpose
%rep 3
    RESET_MM_PERMUTATION
    add r2, 8
    lea r0, [r0 + r1 * 8]
    call transpose
    mov r4, 256
%assign %%i 0
%rep 8
    movq [r2 + r4 + (%%i*32 - 8)], m%%i
%assign %%i %%i+1
%endrep
%endrep
;;transpose last block
    RESET_MM_PERMUTATION
    add r2, 256
    lea r0, [r0 + r1 *8]
    call transpose
    RET
%endif
%endmacro

; ;; Extending this to do a 16 x 16 transpose (assuming mmsize >= 16)
; ;; basically this comes down to transposing a 2x2 matrix, of 8x8 matrices
; ;; transpose top two 8x8 blocks, push onto stack; call A,B
; ;; transpose bottom two 8x8 blocks; call C,D
; ;; for each line
; ;; unpcklqdq (C,D), (A,B) -> (B, D)
; ;; unpckhqdq (C,D), (A,B) -> (A, C)
INIT_MMX mmx2
gen_transpose
INIT_XMM sse2
gen_transpose
INIT_YMM avx2
gen_transpose
