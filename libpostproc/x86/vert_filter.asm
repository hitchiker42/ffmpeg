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
