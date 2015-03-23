;*
;* DeInterlacing filters written using SIMD extensions
;* Copyright (C) 2015 Tucker DiNapoli (T.Dinapoli at gmail.com)
;*
;* Adapted from inline assembly:
;* Copyright (C) 2001-2002 Michael Niedermayer (michaelni@gmx.at)
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
;; All deinterlace functions operate on N 8x8 blocks at a time, where N
;; is the size (in bytes) of the simd registers being used divided
;; by 8, so 2 for xmm, and 4 for ymm.

;; NOTE: The function names are camel case for compatibility with existing 
;; postprocessing code, eventually the names of postprocessing functions
;; will be changed to use lowercase and underscores

;; Deinterlace blocks using linear interpolation
;; Set each line 2n+1 to (line 2n + line 2n+2)/2
%macro gen_deinterlace_interpolate_linear 0
cglobal deInterlaceInterpolateLinear, 2, 4, 2;, src, stride
    lea r0, [r0 + r1 * 4]
    lea r2, [r0 + r1]
    lea r3, [r2 + r1 * 4]
    mova m0, [r0]
    mova m1, [r2 + r1]
    pavgb m0,m1
    mova [r2], m0
    mova m0, [r0 + r1 * 4]
    pavgb m1,m0
    mova [r2 + r1 * 2],m1
    mova m1, [r3 + r1]
    pavgb m0,m1
    mova [r3], m0
    mova m1, [r0 + r1 * 8]
    pavgb m0,m1
    mova [r3 + r1 * 2], m0
    RET
%endmacro
;; Deinterlace blocks using cubic interpolation
;; Line 2n+1 = (9(2n) + 9(2n+2) - (2n-2) - (2n+4))/16
%macro gen_deinterlace_interpolate_cubic 0
cglobal deInterlaceInterpolateCubic, 2, 5, 5;, src, stride
    lea r2, [r1 + r1 * 2]
    add r0,r2
    lea r2, [r0 + r1]
    lea r3, [r2 + r1 * 4]
    lea r4, [r3 + r1 * 4]
    pxor m4,m4
%ifnmacro deint_cubic
;; given 5 lines a,b,c,d,e: a = c-3, b = c-1, d = c+1, e = c + 2
;; set c = (9b + 9d - a - b)/16
%macro deint_cubic 5
    mova m0,%1
    mova m1,%2
    mova m2,%4
    mova m3,%5
    pavgb m1,m2 ;(b+d)/2
    pavgb m0,m3 ;(a+e)/2
    ;; convert each byte into a word
    mova m2,m1
    punpcklbw m1, m4
    punpckhbw m2, m4
    mova m0,m3
    punpcklbw m0, m4
    punpckhbw m3, m4
    ;; calculate the pixel values
    psubw m0, m1 ;L(a+e - (b+d))/2
    psubw m3, m2 ;H(a+e - (b+d))/2
    psraw m0, 3
    psraw m3, 3
    psubw m1, m0 ;L(9(b+d) - (a+e))/16
    psubw m3, m2 ;H(9(b+d) - (a+e))/16
    ;; convert the words back into bytes using unsigned saturation
    packuswb m1, m3
    mova %3, m1
%endmacro
%endif
    deint_cubic [r0], [r2 + r1], [r2 + r1 *2],\
                [r0 + r1 *4], [r3 + r1]
    deint_cubic [r2 + r1], [r0 + r1 * 4], [r3],\
                [r3 + r1], [r0 + r1 * 8]
    deint_cubic [r0 + r1 * 4], [r3 + r1], [r3 + r1 * 2],\
                [r0 + r1 * 8], [r4]
    deint_cubic [r3 + r1], [r0 + r1 * 8], [r3 + r1 * 4],\
                [r4], [r4 + r1 * 2]
    RET
%endmacro

;; deinterlace blocks by seting every line n to (n-1 + 2n + n+1)/4
%macro gen_deinterlace_blend_linear 0
cglobal deInterlaceBlendLinear, 3, 5, 2 ;src, stride, tmp
    lea r0, [r0 + r1 * 4]
    lea r3, [r0 + r1]
    lea r4, [r3 + r1 * 4]
    mova m0, [r2] ;L0
    mova m1, [r3] ;L2
    mova m2, [r0] ;L1
    pavgb m0, m1 ;L0+L2
    pavgb m0, m2 ;L0 + 2L1 + L2 / 4
    mova [r0], m0
    mova m0, [r3 + r1 * 2] ;L3
    pavgb m2, m0
    pavgb m2, m1
    mova [r3], m2 ;L4
    mova m2, [r3 + r1 * 2]
    pavgb m1, m2
    pavgb m1, m0
    mova [r3+r1], m1 ;L5
    mova m1, [r0 + r1 * 4]
    pavgb m0, m1
    pavgb m0, m2
    mova [r3 + r1 * 2], m0 ;L6
    mova m0, [r4]
    pavgb m2, m0
    pavgb m2, m1
    mova [r0 + r1 * 4], m2 ;L7
    mova m2, [r4 + r1]
    pavgb m1, m2
    pavgb m1, m0
    mova [r4], m1
    mova m1, [r4 + r1 * 2]
    pavgb m0, m1
    pavgb m0, m2
    mova [r4 + r1], m0
    mova m0, [r0 + r1 * 8]
    pavgb m2, m0
    pavgb m2, m1
    mova [r4 + r1 * 2], m2
    mova [r2], m1
    RET
%endmacro
;; I'm not exactly sure how to insure the following only get built if
;; the specified instruction set is available.
;; If the INIT_XXX macros do that then great, otherwise I'll correct it
SECTION_TEXT

INIT_MMX mmx2
gen_deinterlace_interpolate_linear
gen_deinterlace_interpolate_cubic
gen_deinterlace_blend_linear

INIT_XMM sse2
gen_deinterlace_interpolate_linear
gen_deinterlace_interpolate_cubic
gen_deinterlace_blend_linear

INIT_YMM avx2
gen_deinterlace_interpolate_linear
gen_deinterlace_interpolate_cubic
gen_deinterlace_blend_linear
