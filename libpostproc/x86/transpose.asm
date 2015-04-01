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

;; For x86_64 using sse/avx we have 16 simd registers, so we can
;; avoid using the stack at all, for x86_32, or if we're using
;; mmx we only have 8 registers, so we need to store intermediate
;; results on the stack
%macro gen_transpose 0
%if ARCH_X86_64 && cpuflags(sse)
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

    mova m8, m0 ;1
    mova m9, m2 ;3
    mova m10, m4 ;5
    mova m11, m6 ;7

;; bytes->words
    punpcklbw m0, m1 ;24,14,23,13,22,12,21,11 = 1,2 L
    punpckhbw m8, m1 ;28,18,27,17,26,16,25,15 = 1,2 H
    mova m1, m0
    mova m12, m8

    punpcklbw m2, m3 ;3,4 L
    punpcklhw m9, m3 ;3,4 H

    punpcklbw m4, m5 ;5,6 L
    punpckhbw m10, m5 ;5,6 L
    mova m5, m4
    mova m13, m10

    punpcklbw m6, m7 ;7,8 L
    punpckhbw m11, m7 ;7,8 H

;; words->doublewords

    punpcklwd m0, m2 ;42,32,22,21,41,31,21,11 = 1,2 L4
    punpckhwd m1, m2 ;44,34,24,14,43,33,32,31 = 3,4 L4
    mova m2, m0
    mova m3, m1

    punpcklwd m4, m6 ;82,72,62,52,81,71,61,51 = 1,2 H4
    punpckhwd m5, m6 ;84,74,64,54,83,73,63,63 = 3,4 H4

    punpcklwd m8, m9 ;5,6 L4
    punpckhwd m12, m9 ;7,8 L4
    mova m6, m8
    mova m7, m12

    punpcklwd m10, m11 ;5,6 H4
    punpckhwd m13, m11 ;7,8 H4

;; doublewords->quadwords

    punpckldq m0, m4 ;81,71,61,51,41,31,21,11 = 1T
    punpckhdq m2, m4 ;82,72,62,52,42,32,22,12 = 2T

    punpckldq m1, m5 ;3T
    punpckhdq m3, m5 ;4T

    punpckldq m6, m10 ;5T
    punpckhdq m8, m10 ;6T

    punpckldq m7, m13 ;7T
    punpckldq m12, m13 ;8T

    lea r4, [r2 + r3]
    lea r5, [r4 + r3]

    mova [r2], m0
    mova [r2 + r3], m2
    mova [r4 + r2], m1
    mova [r4 + 2*r2], m3
    mova [r2 + 4*r3], m6
    mova [r5], m8
    mova [r5 + r3], m7
    mova [r5 + 2*r3], m12

RET
%else ;;ARCH_x86_32 || mmx
cglobal transpose, 4, 6, 8, 4*mmsize ;src,src-stride, dst,dst-stride
    lea r4, [r0 + r1]
    lea r5, [r4 + r1]
    mova m0, [r0] ;1
    mova m1, [r4] ;2
    mova m2, [r0 + 2*r1] ;3
    mova m3, [r4 + 2*r1] ;4

    mova m4, m0 ;1
    mova m5, m2 ;3

;; ij is the j'th element of the i'th row
;; rows 1-4
;; bytes->words
    punpcklbw m0, m1 ;24,14,23,13,22,12,21,11 = 1,2 L
    punpckhbw m4, m1 ;28,18,27,17,26,16,25,15 = 1,2 H

    punpcklbw m2, m3 ;3,4 L
    punpckhbw m5, m3 ;3,4 H

;; we could use m6,m7, and just keep the results there
    mova m1, m0
    mova m3 ,m4
;; words->doublewords
    punpcklwd m0, m2 ;42,32,22,12,41,31,21,11
    punpckhwd m1, m2 ;44,34,24,14,43,33,23,13

    punpcklwd m3, m5 ;46,36,26,16,45,35,25,15
    punpckhwd m4, m5 ;48,38,28,18,47,37,27,17

;; store rows 1-4
    mova [rsp], m0
    mova [rsp + mmsize], m1
    mova [rsp + 2*mmsize], m3
    mova [rsp + 3*mmsize], m4

;; rows 5-8
    mova m0, [r0 + 4*r1] ;5
    mova m1, [r4 + 4*r1] ;6
    mova m2, [r0 + 2*r1] ;7
    mova m3, [r4 + 2*r1] ;8

    mova m4, m0
    mova m5, m2
;; bytes->words
    punpcklbw m0, m1 ;64,54,63,53,62,52,61,51 = 5,6 L
    punpckhbw m4, m1 ;68,58,67,57,66,56,65,55 = 5,6 H

    punpcklbw m2, m3 ;7,8 L
    punpckhbw m5, m3 ;7,8 H

    mova m1, m0
    mova m3, m2
;; words->doublewords
    punpcklwd m0, m2 ;82,72,62,52,81,71,61,51
    punpckhwd m1, m2 ;84,74,64,54,83,73,63,53

    punpcklwd m3, m5 ;86,76,66,56,85,75,65,55
    punpckhwd m4, m5 ;88,78,68,58,87,77,67,57

;; all rows, doublewords->quadwords
;; m2,m5,m6,m7 free
    mova m2, [rsp]
    mova m5, m2
    mova m6, [rsp + mmsize]
    mova m7, m6

    punpckldq m2, m0 ;1T
    punpckhdq m5, m0 ;2T
    punpckldq m6, m1 ;3T
    punpckhdq m7, m1 ;4T

    lea r4, [r2 + r3]
    lea r5, [r4 + r3]

    mova [r2], m2
    mova [r4], m5
    mova [r2 + 2*r3], m6
    mova [r4 + 2*r3], m7

    mova m2, [rsp + 2*mmsize]
    mova m5, m2
    mova m6, [rsp + 3*mmsize]
    mova m7, m6

    punpckldq m2, m3 ;5T
    punpckhdq m5, m3 ;6T
    punpckldq m6, m4 ;7T
    punpckhdq m7, m4 ;8T

    mova [r2 + 4*r3], m2
    mova [r4 + 4*r3], m5
    mova [r5 + r3], m6
    mova [r5 + 2*r3], m7
RET
%endmacro

/*
    transpose and store 5 blocks into dst as follows:
    |1|2|3|4|
    |2|3|4|5|
    this allows 2 consuctive blocks to be accessed, which
    is necessary for some functions
*/
/*
    Extract core routine for transposing into macro
avx:
    transpose 4 blocks starting at src, store in m0-m7.
    for(i=0;i<7;i++){
      mov [dst + 8*8*4*i],m%i
;; second 2 blocks in the second row
      vextracti128  [dst + 8*8*4*(8+i) + 64], m%i, 1
;; 1st block in second row
      vpextrq [dst + 8*8*4*(*+i)], m%i, 1
    }
    ;;if there is another block in src
    ;;transpose it and store it in the last block in the second row
    ;;if not figure out something
sse:
    transpose 2 blocks starting at src, store in m0-m7
    for(i=0;i<8;i++){
      mov [dst + 8*8*4*i],m%i
      pextrq [dst + 8*8*4*(8+i),m%i, 1
    }
    transpose 2 blocks starting at src+128
    for(i=0;i<8;i++){
      mov [dst + 8*8*4*i + 128], m%i
      movu [dst + 8*8*4*(8+i) + 64], m%i
    }
    ;;follow same rule as avx for last block
mmx:
    transpose block 1, store at dst+0
    transpose block 2, store at dst+64, dst+256*8
    transpose block 3, store at dst+128, dst +256*8 + 64
    transpose block 4, store at dst+196, dst+256*8 + 128
    ;;follow same rule for last block
*/
%macro gen_transpose_shift
cglobal transpose_shift 3,6,8,4*mmsize ;;src, stride, dst, [dstStride]
    lea r4, [r0 + r1]
    lea r5, [r4 + 4*r1]
;; code follows the same idea as the two functions above
;; so detailed comments indicating the state are ommited
    mova m0, [r0] ;1
    mova m1, [r4] ;2
    mova m2, [r4 + r1] ;3
    mova m3, [r4 + 2*r1] ;4
    mova m4, m0
    mova m5, m2


    punpcklbw m0, m1 ;;1,2 L
    punpckhbw m4, m1 ;;1,2 H
    punpcklbw m2, m3 ;;3,4 L
    punpckhbw m5, m3 ;;3,4 H

    mova m1, m0
    mova m3, m4

    punpcklwd m0, m2 ;;42,32,22,12,41,31,21,11
    punpckhwd m1, m2 ;;44,34,24,14,43,33,23,13
    punpcklwd m3, m5 ;;46,36,26,16,45,35,25,15
    punpckhwd m4, m5 ;;48,38,28,18,47,37,27,17

;; here's where this gets weird
;; this only works for mmx, right now
    movd  [r2 + 128], m0 ;;41,31,21,11
    psrlq m0, 32
    movd [r2 + 144], m0 ;;42,32,22,12
    movd [r2 + 160], m1 ;;43,33,23,13
    psrlq m1, 32
    movd [r2 + 176], m1
    movd [r3 + 48], m1 ;;44,34,24,14
    movd [r2 + 192], m3 ;;45-15
    movd [r3 + 64], m3 ;;45-15
    psrlq m3, 32
    movd [r3 + 80], m3;;46-16
    movd [r3 + 96], m4
    psrlq m4, 32
    movd [r3 + 112];;still what?

    mova m0, [r0 + r1 * 4] ;;58,57,56,55,54,53,52,51
    mova m1, [r5] ;;68,67,66,65,64,63,62,61
    mova m2, [r5 + r1] ;7
    mova m3, [r5 + 2*r1] ;8
    mova m4, m0
    mova m5, m2


    punpcklbw m0, m1 ;;64,54,63,53,62,52,61,51 = 5,6 L
    punpckhbw m4, m1 ;;68,58,67,57,66,56,65,55 = 5,6 H
    punpcklbw m2, m3 ;;7,8 L
    punpckhbw m5, m3 ;;7,8 H

    mova m1, m0
    mova m3, m4

    punpcklwd m0, m2 ;;82,72,62,52,81,71,61,51
    punpckhwd m1, m2 ;;84,74,64,54,83,73,63,53
    punpcklwd m3, m5 ;;86,76,66,56,85,75,65,55
    punpckhwd m4, m5 ;;88,78,68,58,87,77,67,57


;;more weirdness
    movd [r2 + 132], m0 ;;81-11
    psrlq m0, 32
    movd [r2 + 148], m0 ;;82-12
;;    ...
    psrlq m1, 32
    mova [r2 + 180], m1 ;;84-14
    movd [r3 + 52], ;;ditto
;;    ...

/*

    so we have
tmpblock1
r2 + 128 81-11
r2 + 136 16_1-91
r2 + 144 82-12
r2 + 152 16_2-92
r2 + 160 83-13
r2 + 168 16_3-93
r2 + 176 84-14 ;;end here
r2 + 182 16_4-94
r2 + 190 85-15

tmpblock2
r3 + 48 84-14 ;;start here
r3 + 56 nothing
r3 + 64 85-15
r3 + 72 nothing
r3 + 80 86-16
r3 + 88 nothing
r3 + 96 87-17
r3 + 104 nothing
r3 + 112 88-18

r3 = r2 + 8


r2 + 56 84-14
r2 + 64 nothing
r2 + 72 85-15
r2 + 80 nothing
r2 + 88 86-16
r2 + 96 nothing
r2 + 104 87-17
r2 + 112 nothing
r2 + 120 88-18

r2 + 128 81-11
r2 + 136 nothing
r2 + 144 82-12
r2 + 152 nothing
r2 + 160 83-13
r2 + 168 nothing
r2 + 176 84-14
r2 + 184 nothing
r2 + 192 85-15

after 1 iteration
xchg tmpblock1, tmpblock2
i.e r2 +=8
    r3 -=8


;; Extending this to do a 16 x 16 transpose (assuming mmsize >= 16)
;; basically this comes down to transposing a 2x2 matrix, of 8x8 matrices
;; transpose top two 8x8 blocks, push onto stack; call A,B
;; transpose bottom two 8x8 blocks; call C,D
;; for each line
;; unpcklqdq (C,D), (A,B) -> (B, D)
;; unpckhqdq (C,D), (A,B) -> (A, C)
