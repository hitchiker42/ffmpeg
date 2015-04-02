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

;; Deinterlace blocks using linear interpolation
;; Set each line 2n+1 to (line 2n + line 2n+2)/2
%macro gen_deinterlace_interpolate_linear 0
cglobal deInterlaceInterpolateLinear, 2, 4, 5;, src, stride
    lea r0, [r0 + r1 * 4]
    lea r2, [r0 + r1]
    lea r3, [r2 + r1 * 4]

    mova m0, [r0] ;0
    mova m1, [r0 + 2*r1] ;2
    mova m2, [r0 + 4*r1] ;4
    mova m3, [r3 + r1] ;6
    mova m4, [r0 + 8*r1] ;8

    pavgb m0, m1 ;1
    pavgb m1, m2 ;3
    pavgb m2, m3 ;5
    pavgb m3, m4 ;7

    mova [r2], m0
    mova [r2 + r1 * 2], m1
    mova [r3], m0
    mova [r3 + r1 * 2], m0
    RET
%endmacro
;; Deinterlace blocks using cubic interpolation
;; Line 2n+1 = (9(2n) + 9(2n+2) - (2n-2) - (2n+4))/16
%macro gen_deinterlace_interpolate_cubic 0
cglobal deInterlaceInterpolateCubic, 2, 5, 5;, src, stride
    lea r2, [r1 + r1 * 2]
    add r0, r2
    lea r2, [r0 + r1]
    lea r3, [r2 + r1 * 4]
    lea r4, [r3 + r1 * 4]
    pxor m4, m4

;; TODO: See if there is speed gained by interleaving invocations of
;; deint_cubic when we have more registers. (i.e doing the computations
;; of two lines at once).
%ifnmacro deint_cubic
;; given 5 lines a,b,c,d,e: a = c-3, b = c-1, d = c+1, e = c + 2
;; set c = (9b + 9d - a - b)/16
%macro deint_cubic 5;;a,b,c,d,e
    mova m0,%1
    mova m1,%2
    mova m2,%4
    mova m3,%5
    pavgb m1,m2 ;(b+d)/2
    pavgb m0,m3 ;(a+e)/2

    mova m2,m1
    punpcklbw m1, m4
    punpckhbw m2, m4

    mova m0,m3
    punpcklbw m0, m4
    punpckhbw m3, m4

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
cglobal deInterlaceBlendLinear, 3, 5, 3 ;src, stride, tmp
    lea r0, [r0 + r1 * 4]
    lea r3, [r0 + r1]
    lea r4, [r3 + r1 * 4]

    mova m0, [r2] ;L0 (tmp)
    mova m1, [r3] ;L2
    mova m2, [r0] ;L1
    pavgb m0, m1 ;L0+L2
    pavgb m0, m2 ;L0 + 2L1 + L2 / 4
    mova [r0], m0

    mova m0, [r3 + r1] ;L3
    pavgb m2, m0
    pavgb m2, m1
    mova [r3], m2

    mova m2, [r3 + r1 * 2]  ;L4
    pavgb m1, m2
    pavgb m1, m0
    mova [r3+r1], m1

    mova m1, [r0 + r1 * 4]  ;L5
    pavgb m0, m1
    pavgb m0, m2
    mova [r3 + r1 * 2], m0

    mova m0, [r4]  ;L6
    pavgb m2, m0
    pavgb m2, m1
    mova [r0 + r1 * 4], m2

    mova m2, [r4 + r1]  ;L7
    pavgb m1, m2
    pavgb m1, m0
    mova [r4], m1 

    mova m1, [r4 + r1 * 2] ;L8
    pavgb m0, m1
    pavgb m0, m2
    mova [r4 + r1], m0

    mova m0, [r0 + r1 * 8] ;L9
    pavgb m2, m0
    pavgb m2, m1
    mova [r4 + r1 * 2], m2

    mova [r2], m1 ;tmp
    RET
%endmacro
;;set every other line Ln to  (-(Ln-2) + 4(Ln-1) + 2(2Ln) + 4(Ln+1) -(Ln+2))/8
%macro gen_deinterlace_FF 0
cglobal deInterlaceFF, 3, 5, 8 ;;src, stride, tmp
    lea r0, [r0 + 4*r1]
    lea r3, [r0 + r1]
    lea r4, [r3 + r1 * 4]
    pxor m7, m7
    mova m6, [r2] ;;L0 (tmp) 

%ifnmacro deint_ff
%macro deint_ff 5
    mova m0, %1 ;;Ln-2
    mova m1, %2 ;;Ln-1
    mova m2, %3 ;;Ln
    mova m3, %4 ;;Ln+1
    mova m4, %5 ;;Ln+2
    mova m6, m2 ;;Ln-2 for next n

    pavgb m1, m3 ;;(Ln-1 + Ln+1)/2
    pavgb m0, m4 ;;(Ln-2 + Ln+2)/2
    mova m3, m0
    mova m4, m1
    mova m5, m2

;; Do calculations on 16 bit values
    punpcklbw m0, m7
    punpckhbw m3, m7
    punpcklbw m1, m7
    punpckhbw m4, m7
    punpcklbw m2, m7
    punpckhbw m5, m7

    psllw m1, 2
    psllw m4, 2 ;;(Ln-1 + Ln+1)*2

    psubw m1, m0
    psubw m4, m3 ;;(Ln-1 + Ln+1)*2 - (Ln-2 + Ln+2)/2
    
    paddw m1, m2
    paddw m4, m5 ;;(Ln-1 + Ln+1)*2 + Ln - (Ln-2 + Ln+2)/2

    psraw m1, 2
    psraw m4, 2 ;;(4(Ln-1 + Ln+1) + 2Ln - (Ln-2 + Ln+2))/8

    packuswb m1, m4
    mova %3, m1
%endmacro
%endif    
    deint_ff m6, [r0], [r3], [r3 + r1], [r3 + r1 * 2]
    deint_ff m6, [r3 + r1], [r3 + r1 *2], [r0 + r1 * 4], [r4]
    deint_ff m6, [r0 + r1 * 4], [r4], [r4 + r1], [r4 + r1 * 2]
    deint_ff m6, [r4 + r1], [r4 + r1 * 2], [r0 + r1 * 8], [r4 + r1 * 4]
    RET
%endmacro

%macro gen_deinterlace_L5 0
;; set each line Ln to (-(Ln-2) + 2(Ln-1) + 6Ln + 2(Ln+1) -(Ln+2))/8
cglobal deInterlaceL5, 4, 6, 8 ;;src, stride, tmp1, tmp2
    lea r0, [r0 + r1 * 4]
    lea r5, [r0 + r1]
    lea r6, [r5 + r1 * 4]
    pxor m7, m7
    mova m0, [r3] ;;Ln-2 (tmp1)
    mova m1, [r4] ;;Ln-1 (tmp2)

%ifnmacro deint_L5
%macro deint_L5 5 ;;Ln-2, Ln-1, Ln, Ln+1, Ln+2
    mova m2, %3;%3
    mova m3, %4;%4
    mova m4, %5;%5
    
    pavgb m4, %1 ;;(Ln-2 + Ln+2)/2
    pavgb m3, %2 ;;(Ln-1 + Ln+1)/2
    
    mova %1, m2 ;;Ln-2 for next n
    mova m5, m2
    mova m6, m3

    punpcklbw m2, m7
    punpckhbw m5, m7

    mova m6, m2
    paddw m2, m2 
    paddw m2, m6
    mova m6, m5
    paddw m5, m5
    paddw m5, m6 ;;3*Ln

    mova m6, m3
    punpcklbw m3, m7
    punpckhbw m6, m7
    
    paddw m3, m3
    paddw m6, m6
    paddw m3, m2
    paddw m6, m5 ;;Ln-1 + 3Ln + Ln+1

    mova m6, m4
    punpcklbw m4, m7
    punpckhbw m6, m7
    
    psubsw m2, m4
    psubsw m5, m6 ;;(-Ln-2 + 2Ln-1 + 6Ln + 2Ln+1 - Ln+2)/2
    psraw m2, 2
    psraw m5, 2 ;;(...)/8 (same as above)
    
    packuswb m2, m5
    mova %3, m2
%endmacro
%endif
    deint_L5 m0, m1, [r0], [r4], [r4 + r1]
    deint_L5 m1, m0, [r4], [r4 + r1], [r4 + r1 * 2]
    deint_L5 m0, m1, [r4 + r1], [r4 + r1 * 2], [r0 + r1 * 4]
    deint_L5 m1, m0, [r4 + r1 * 2], [r0 + r1 * 4], [r5]
    deint_L5 m0, m1, [r0 + r1 * 4], [r5], [r5 + r1]
    deint_L5 m1, m0, [r5], [r5 + r1], [r5 + r1 * 2]
    deint_L5 m0, m1, [r5 + r1], [r5 + r1 * 2], [r0 + r1 * 8]
    deint_L5 m1, m0, [r5 + r1 * 2], [r0 + r1 * 8], [r5 + r1 * 4]
    RET
%endmacro

%macro gen_deinterlace_median 0
;; Apply a median filter to every second line
;; i.e for each set of bytes a,b,c in Ln-1,Ln,Ln+1 
;; set d,e,f equal to a,b,c such that d <= e <= f, set the byte in Ln equal to d
cglobal deInterlaceMedian, 2, 4, 4 ;;src, stride
    lea r0, [r0 + r1 * 4]
    lea r2, [r0 + r1]
    lea r3, [r2 + r1 * 4]
%ifnmacro deint_median    
%macro deint_median 4
    mova %4, %1
    pmaxub %1, %2
    pminub %2, %4
    pmaxub %2, %3
    pminub %1, %2
%endmacro
%endif
    mova m0, [r0] ;0
    mova m1, [r2] ;1
    mova m2, [r2 + r1] ;2
    deint_median m0, m1, m2, m3
    mova [r2], m0 ;1
    
    ;; m2 = 2
    mova m1, [r2 + r1 * 2] ;3
    mova m0, [r0 + r1 * 4] ;4
    deint_median m2, m1, m0, m3
    mova [r2 + r1 * 2], m2 ;3


    ;; m0 = 4
    mova m2, [r3] ;5
    mova m1, [r3 + r1] ;6
;;This confuses me, why isn't m0, m1, m2, m3
    deint_median m2, m0, m1, m3
    mova [r3], m2 ;5

    mova m2, [r3 + r1 * 2] ;7
    mova m0, [r0 + r1 * 8] ;8
;; and shouldn't this be m1, m0, m2, m3 
    deint_median m2, m1, m0, m3
    mova [r3 + r1 * 2], m2 ;7
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
gen_deinterlace_FF
gen_deinterlace_L5
gen_deinterlace_median

INIT_XMM sse2
gen_deinterlace_interpolate_linear
gen_deinterlace_interpolate_cubic
gen_deinterlace_blend_linear
gen_deinterlace_FF
gen_deinterlace_L5
gen_deinterlace_median

INIT_YMM avx2
gen_deinterlace_interpolate_linear
gen_deinterlace_interpolate_cubic
gen_deinterlace_blend_linear
gen_deinterlace_FF
gen_deinterlace_L5
gen_deinterlace_median
