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
cglobal vert_classify, 3, 6, 7;,src, stride, context
;;nonBQP is (or will be) an array of 4 bytes
;;presumably mmx_dc_offset/threshold (an array of quadwords) will stay the same
;;but, we will need to load 2-4 elements from that array for sse/avx respectively
    mov r3d, [r2 + PPContext.nonBQP]
    lea r4, [r2 + PPContext.mmx_dc_offset]
%if cpuflag(avx2)
    ;;vpinsrq can only insert into the low 128 bits, so
    ;;use two different registers to hold the 4 values and
    ;;blend them together
    vpinsrq m0, [r4 + r3b * 8], 0
    shr r3, 8
    vpinsrq m0, [r4 + r3b * 8], 1
    shr r3, 8
    vpinsrq m7, [r4 + r3b * 8], 0
    shr r3, 8
    vpinsrq m7, [r4 + r3b * 8], 1
    vpermq m7, m7, 0b01000100
    vpblendd m7, m0, 0b11110000
%elif cpuflag(sse2)
%else ;;mmx
%endif
;;repeat for dc_threshold

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
cglobal doVertLowPass 3, 5, 8 ;;src, stride, ppcontext
    lea r3, [r1 + r1 * 4]
    lea r0, [r0 + r3]
    lea r4, [r3 + r1 * 4];;r0 + r1 * 7
    lea r3, [r0 + r1 * 4]
    lea r0, [r0 + r1]

    mova m0, [r2 + PPContext.pQPb] ;;may change field to pQPb_block
    pxor m7, m7

    mova m1, [r0] ;;L0
    mova m2, [r0 + rl] ;;L1
    mova m5, m1
    mova m6, m2

    psubusb m5, m2
    psubusb m2, m1
    por m2, m5

    psubusb m2, m0
    pcmpeqb m2, m7 ;;for each byte if |L1-L2| < QP set m2 to 0xff

    pand m6, m2 ;;if |L1-L2| < QP take the byte from L2
    pandn m2, m1 ;;otherwise take it from L1
    por m6, m2 ;;L0/1

    ;;do the same thing again for different lines

    mova m4, [r4 + r1];;L8
    mova m5, [r4 + r1 * 2];;L9
    mova m4, m1
    mova m5, m2

    psubusb m4, m2
    psubusb m2, m1
    por m2, m4

    psubusb m2, m0
    pcmpeqb m2, m7

    pand m5, m2
    pandn m2, m1
    por m5, m2 ;;L8/9
;; Relative to the inline asm m4->m7, m5->m4, m7->m5
    mova m0, [r0 + r1] ;;L2
    mova m1, m0
    pavgb m0, m6
    pavgb m0, m6 ;;(3L2 + L0/1) / 2

    mova m2, [r0 + r1 * 4] ;;L5
    mova m4, m2
    pavgb m2, [r3] ;;(L4+L5)/2
    pavgb m2, [r0 + r1 * 2] ;;(L4 + L5 + 2*L3)/4
    mova m3, m2 ;;(L4 + L5 + 2*L3)/4

    mova m7, [r0] ;;L1
    pavgb m3, m7 ;;(4L1 + L4 + L5 + 2*L3)/8
    pavgb m3, m0 ;;(4L1 + L4 + L5 + 2*L3 + 6L2 + 2L0/1)/16
    mova [r0], m3 ;;L1 (store)

    mova m0, m1 ;;L2
    mova m3, m7 ;;L1
    pavgb m0, m6 ;;(L2 + L0/1)/2
    pavgb m3, [r0 + r1 * 2] ;;(L1 + L3)/2
    pavgb m4, [r3 + r1 * 2] ;;(L5 + L6)/2

    pavgb m4, [r3] ;;(2L4 + L5 + L6)/4
    pavgb m3, m4 ;;(2L1 + 2L3 + 2L4 + L5 + L6)/8
    pavgb m3, m0 ;;(4L0/1 + 2L1 + 4L2 + 2L3 + 2L4 + L5 + L6)/16
    mova [r0 + r1], m3 ;;L2 (store)

    mova m0, [r4] ;;L7
    pavgb m6, m7 ;;(L1 + L0/1)/2
    pavgb m0, [r3 + r1 * 2] ;;(L7 + L6)/2
    mova m3, m0
    pavgb m0, m1 ;;(2L2 + L7 + L6)/4
    pavgb m0, m6 ;;(2L1 + 2L0/1 + 2L2 + L7 + L6)/8
    pavgb m0, m2 ;;(2L4 + 2L5 + 4*L3 + 2L1 + 2L0/1 + 2L2 + L7 + L6)/16
    mova m2, [r0 + r1 * 2] ;;L3
    mova [r0 + r1 * 2], m0 ;;L3 (store)

    mova m0, [r3 + r1 * 4] ;;L8
    pavgb m0, [r4]  ;;(L7 + L8)/2
    pavgb m0, m6 ;;(L1 + L0/1 + L7 + L8)/4
    pavgb m7, m1 ;;(L1 + L2)/2
    pavgb m1, m2 ;;(L2 + L3)/2
    pavgb m6, m1 ;;(L0/1 + L1 + 2L2 + 2L3 + L7 + L8)/8
    pavgb m6, m4 ;;(L0/1 + L1 + 2L2 + 2L3 + 4L4 + 2L5 + 2L6 + L7 + L8)/16
    mova m4, [r3] ;;L4
    mova [r3], m6 ;;L4 (store)
    mova m6, [r3 + r1 * 4];;L8


    pavgb m6, m5 ;;(L8/9 + L8)/2
    pavgb m6, m7 ;;(L1 + L2 + L8 + L8/9)/4
    pavgb m6, m3 ;;(L1 + L2 + 2L6 + 2L7 + L8 + L8/9)/8
    pavgb m2, m4 ;;(L3 + L4)/2

    mova m7, [r0 + r1 * 4] ;;L5
    pavgb m2, m7 ;;(L3 + L4 + 2L5)/4
    pavgb m6, m2 ;;(L1 + L2 + 2L3 + 2L4 + 4L5 + 2L6 + 2L7 + L8 + L8/9)/16
    mova [r0 + r1 * 4], m6 ;;L5 (store)

    pavgb m1, m5 ;;(L2 + L3 + 2L8/9)/4
    pavgb m4, m7 ;;(L4 + L5)/2
    pavgb m0, m4 ;;(L4 + L5 + L7 + L8)/4
    mova m6, [r3 + r1 * 2] ;;L6
    pavgb m1, m6 ;;(L2 + L3 + 4L6 + L8/9)/8
    pavgb m1, m0 ;;(L2 + L3 + 2L4 + 2L5 + 4L6 + 2L7 + 2L8 + L8/9)/16
    mova [r3 + r1 * 2], m1 ;;L6 (store)

    mova m0, [r3 + r1 * 4] ;;L8
    pavgb m2, [r4] ;;(L3 + L4 + 2L5 + 4L7)/8
    pavgb m6, m0 ;;(L6 + L8)/2
    pavgb m6, m5 ;;(L6 + L8 + 2L8/9)/4
    pavgb m6, m2 ;;(L3 + L4 + 2L5 + 2L6 + 4L7 + 2L8 + 4L8/9)/16
    mova [r4], m6 ;;r7 (store)

    pavgb m5, m4 ;;(L4 + L5 + 2L8/9)/4
    pavgb m5, m4 ;;(L4 + L5 + 6L8/9)/8
    pavgb m0, m3 ;;(L6 + L7 + 2L8)/4
    pavgb m4, m0 ;;(L4 + L5 + 2L6 + 2L7 + 4L8 + 6L8/9)/16
    mova [r3 + r1 * 4], m4 ;;L8 (store)
    sub r0, r1 ;;Why do this, to set flags? I can't think of another reason
    RET
%endmacro

%macro gen_do_vert_def_filter 0
cglobal doVertDefFilter 3,6,8 ;src,stride,ppcontext
    lea r0, [r0 + 4*r1]
    lea r3, [r0 + r1]
    lea r4, [r3 + 4*r1]
    pcmpeQb m7, m7 ;;all 1s
    mova m0, [r0 + 4*r1] ;L4
    pxor m6, m6
    mova m1, [r3 + 2*r1] ;L3
    mova m2, [r3 + 4*r1] ;L5
    mova m3, [r3 + r1]   ;L2
    mova m4, [b80] ;each byte is 0x80
    pxor m1, m7 ;~L3 = -L3 -1
    pxor m2, m7 ;~L5 = -L5 -1
    mova m5, m2

    pavgb m0, m1 ;;(L4-L3+256)/2 === 128-Q
    pavgb m2, m3 ;;(L2-L5+256)/2
    pavgb m4, m0 ;;~(L4-L3)/4 + 128
    pavgb m4, m2 ;;~(L2-L5)/4 + (L4-L3)/8 + 128
    pavgb m4, m0 ;;~(L2-L5)/8 + 5(L4-L3)/16 + 128 === M/16

    mova m2, [r3] ;;L1
    pxor m2, m7  ;;~L1 = -L1-1
    pavgb m2, m3 ;;(L2-L1+256)/2
    pavgb m1, [r0] ;;(L0-L3+256)/2
    mova m3, [b80]
    pavgb m3, m2 ;;~(L2-L1)/4 + 128
    pavgb m3, m1 ;;~(L0-L3)/4 + (L2-L1)/8 + 128
    pavgb m3, m2 ;;~(L0-L3)/8 + 5(L2-L1)/16 + 128 === L/16

    pavgb m5, [r4 + r1] ;;(L6 - L5 + 256)/2
    mova m1, [r4 + r1 * 2];;L7
    pxor m1, m7 ;;~L7
    pavgb m1, [r0 + r1 * 4] ;;(L4-L7+256)/2
    mova m2, [b80]
    pavgb m2, m5 ;;~(L6-L5)/4 + 128
    pavgb m2, m1 ;;~(L4-L7)/4 + (L6-L5)/8 + 128
    pavgb m2, m5 ;;~(L4-L7)/8 + 5(L6-L5)/16 + 128 === R/16

    pxor m1, m1
    pxor m5, m5

    psubb m1, m2 ;;128 - R/16
    psubb m5, m3 ;;128 - L/16
    pmaxub m2, m1 ;;128 + |R/16|
    pmaxub m3, m5 ;;128 + |L/16|
    pminub m3, m2 ;;128 + min(|L|,|R|)/16

    mova m2, [r2 + PPContext.pQPb]
    pavgb m2, m7 ;;128+QP/2
    psubb m6, m2 ;;QP/2 (is this the best way to get this?)

    mova m1, m4 ;;M/16
    pcmpgtb m1, m6 ;;SGN(M)
    pxor m4, m1 ;;|M|/16
    psubb m4, m1 ;;128 + |M|/16
    pcmpgtb m2, m4 ;;|M|/16 < QP/2 (mask)
    psubusb m4, m3 ;;|M|/16 - MIN(|L|,|R)/16 === D/16

    mova m3, m4 ;;D/16
    psubusb m4, [b01] ;;D/16 - 1
;;The way pavgb works is that it calculates the sum with 9 bit
;;precision, adds 1 and then shifts right by 1, so pavgb x, 0
;;is equivlent to (x+1)/2, this should explain the next 2 results
    pavgb m4, m6 ;;D/32
    pavgb m4, m6 ;;(D+32)/64 == (D/32 + 1)/64
    paddb m4, m3 ;;5d/64 (I guess the 32 goes away due to rounding)
    pand m4, m2 ;;|M|/16 < QP/2 ? 5d/64 : 0

    mova m5, [b80]
    psubb m5, m0 ;;128 - (L4-L3+256)/2 === 128 - (128-Q) === Q
    paddsb m5, m7 ;;fix bad rounding ??? (Q - 1)
    pcmpgtb m6, m5 ;;sgn(Q)
    pxor m5, m6  ;;abs(Q)

    pminub m4, m5 ;;min(|Q|,5d/64)
    pxor m6, m1 ;;SGN(D*Q) ??? (sgn(Q) ^ sgn(M))


    pand m4, m6 ;;SGN(D*Q) == 1 && |M|/16 < QP/2 ? 5d/64 : 0 === X
    mova m0, [r3 + r1 * 2] ;;L3
    mova m2, [r0 + r1 * 4] ;;L4
    pxor m0, m1 ;;L3 ^ SGN(M) ?
    pxor m2, m1 ;;L4 ^ SGN(M) ?
    paddb m0, m4 ;;X + (L3 ^ SGN(M)
    paddb m2, m4 ;;X + (L4 ^ SGN(M)
    pxor m0, m1 ;;(X + (L3 ^ SGN(M))^SGN(M)
    pxor m2, m1 ;;(X + (L4 ^ SGN(M))^SGN(M)
    mova [r3 + r1 * 2], m0 ;;(X + (L3 ^ SGN(M))^SGN(M)
    mova [r0 + r1 * 4], m2 ;;(X + (L4 ^ SGN(M))^SGN(M)
    RET
;; In postprocess_template there's a seperate implementation
;; For MMX without pavgb
%endmacro
