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
