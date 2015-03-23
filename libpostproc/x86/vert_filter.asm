;******************************************************************************
;*
;* Copyright (c) 2015 Tucker DiNapoli
;*
;* Algorithms from existing postprocessing code by Michael Niedermayer
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
%include "PPutil.asm"

;; The return value is a 32 bit integer with each byte corrsponding to a block
%macro gen_vert_classify 0
cglobal vert_classify, 3, 5, 7;,src, stride, context, tmp1, tmp2
;; If there's a better way to load these values feel free to change this
;; I assume the mmx_dc_offset and mmx_dc_threshold will need to be changed
;; for sse2/avx2 code, as in the commented out code below
; %if cpuflag(avx2)
;     lea r3, [r2 + PPContext.nonBQP * 4];assuming 256 bit elements
; %elif cpuflag(sse2)
;     lea r3, [r2 + PPContext.nonBQP * 2];assuming 128 bit elements
; %else ;this is how the code currently is
    lea r3, [r2 + PPContext.nonBQP]
;%endif
    lea r4, [r2 + PPContext.mmx_dc_offset]
    movq m7, [r4 + r3 * 8]
    lea r4, [r2 + PPContext.mmx_dc_threshold]
    movq m6, [r4 + r3 * 8]
    dup_low_quadword m6
    dup_low_quadword m7

    lea r3, [r0 + r1]
    mova m0, [r0]
    mova m1, [r3]
    mova m3, m0
    mova m4, m0
;; m0 keeps a tally of which lines differ (by at least mmx_dc_threshold)
;; m3 and m4 keep track of the minimum and maximum values over all the lines
    pmaxub m4, m1
    pminub m3, m1
    psubb m0, m1
    paddb m0, m7
    pcmpgtb m0, m6


    mova m2, [r3 + r1]
%macro do_line 2
    pmaxub m4, %1
    pminub m3, %1
    psubb %2, %1 ;x = last line - current line
    paddb %2, m7 ;x += mmx_dc_offset
    pcmpgtb %1, m6 ;x = x > mmx_dc_threshold (set element in x to 0xFF if true)
    paddb m0, %1 ;m0 += x
%endmacro
    do_line m2, m1

    mova m1, [r3 + r1 * 2]
    do_line m1, m2

    lea r3, [r3 + r1 * 4]
    mova m2, [r0 + r1 * 4]
    do_line m2, m1

    mova m1, [r3]
    do_line m1, m2

    mova m2, [r3 + r1]
    do_line m2, m1

    mova m1, [r3 + r1 * 2]

    do_line m1, m2
    psubusb m4, m3 ;substract w/unsigned saturation
    pxor m7, m7
;; compute the sum of absolute diferences of each 8 bytes, and store the results
;; into the low 16 bits of each 64 bit section
    psadbw m0, m7
    movq m7, [r2 + PPContext.pQPb]
;; copy low quadword to upper quadword(s)
    dup_low_quadword m7
    paddusb m7, m7
    psubusb m4, m7

    packssdw m4, m4 ;pack doublewords into words with signed saturation
    pxor m3, m3
    pcmpeqb m1,m1 ;set all elements of m1 to 1
;; Is there a better way to negate a simd register?
    psub m0, m3
    pand m0, m1 ; now m0 = -m0 & 0xff...
    movd m2, [r2 + (PPContext.ppMode + PPMode.flatness_threshold)]
;; This should  the low 32 bits of each 64 bit element in m2 to the restult
;; of the above instruction I'm not entierly sure this will work b/c of endianess
    dup_low_quadword m2
;; The return value is as follows (for each quadword):
;;  m0 > m2 ? m4 ? 0 : 1 : 2
;; set m5 to packed doublewords equal to 1, and m6 to doublewords equal to 2
    pcmovled m3, m6, m2, m0, m0 ;set each quadword in m3 to 2 if m0 <= m2
    pandn m0, m1 ;;flip bits of m0 (m0 is the mask from the previous comparison)
    pxor m2, m2
    pcmoveqd m2, m5, m2, m4, m4 ;;set quadwords of m2 to 1 if m4 != 0
    pand m2, m0 ;;set the quadwords that have been set to 2 in m3 to 0 in m2
    paddd m3, m2
;; now each quadword in m3 should be set to the proper return value
;; now pack the return value into eax (one byte per block)
%if cpuflag(avx2)
;; There are too many avx intsructions I don't know what to use
%elif cpuflag(sse2)
    pshufd m3, m3 00101011b
;; not super efficent, but it should work
    pextrw eax, m3, 0x00
    pextrw edi, m3, 0x02
    sll edi, 8
    or eax, edi
%if
%else ;cpuflag(mmx), 32 bit by default
    movd eax, m3
%endif
    RET
%endmacro
%macro gen_do_vert_low_pass 0
%endmacro
%macro gen_vert_X1_filter 0
%endmacro
%macro gen_do_vert_def_filter 0
%endmacro
;;

    ; pmaxub m4, m1
    ; pminub m3, m1
    ; psubb m2, m1
    ; paddb m2, m7
    ; pcmpgtb m2, m6
    ; paddb m0, m2

    ; pmaxub m4, m2
    ; pminub m3, m2
    ; psubb m1, m2
    ; paddb m1, m7
    ; pcmpgtb m1, m6
    ; paddb m0, m1
