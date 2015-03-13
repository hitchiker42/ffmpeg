;******************************************************************************
;*
;* Copyright (c) 2015 Tucker DiNapoli
;*
;* De-Ring filer
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
%include "PPUtil.asm"
%assign b01 0x0101010101010101
%assign b02 0x0202020202020202
%assign b08 0x0808080808080808
%assign b80 0x8080808080808080
;;
define_qword_vector_constant dering_threshold,20,20,20,20
%macro find_min_max 1
    mova m0, %1
    pminub m7, m0
    pmaxub m6, m0
%endmacro
;; shuffles the words in each quadword of %1,%2 according to %3
%macro shuffle_words 3
%if cpuflag(sse2) | cpuflag(avx2)
    pshuflw %1,%2,%3
    pshufhw %1,%2,%3
%else
    pshufw %1,%2,%3
%endif
%endmacro
%macro gen_dering
;; 
%assign %%stacksz 4*mmsize
cglobal dering, 3, 6, 7;src, stride, context, tmp1, tmp2, tmp3
%if ARCH_X86_64 == 0
    sub esp,(4 * %%stacksz)
    lea r6,esp
%elif WIN64
    sub rsp,(4 * %%stacksz)
    lea r6,rsp
%else
    lea r6,[rsp + %%stacksz]
%endif
    pxor m6,m6
    pcmpeqb m7,m7 ;set to 1s
;; Not sure what this does
    movq m0, [r3 + PPContext.pQPb]
    dup_low_quadword m0
    punpcklbw m0, m6
    psrlw m0, 1
    psubw m0, m7
    packuswb m0, m0
    movq m0, [r3 + PPContext.pQPb2]
;; end not sure part
    mov r4, [r1 + r2]
    mov r5, [r4 + r2 * 4]
;; Would it be more efficent to use different registers for different lines
;; or can the cpu tell that m1-m5 aren't being used and alias them to m0?
    find_min_max [r4]
    find_min_max [r4 + r2]
    find_min_max [r4 + r2 * 2]
    find_min_max [r1 + r2 * 4]
    find_min_max [r5]
    find_min_max [r5 + r2]
    find_min_max [r5 + r2 * 2]
    find_min_max [r1 + r2 * 8]
;; m6 holds the min of the rows, m7 the max
    mova m4, m7
    psrlq m7, 8
    pminub m7, m4
    shuffle_words m4, m7, 11111001b
    pminub m7, m4
    shuffle_words m4, m7, 11111110b
    pminub m7, m4

    mova m6, m4
    psrlq m6, 8
    pmaxub m6, m4
    shuffle_words m4, m6, 11111001b
    pmaxub m6, m4
    shuffle_words m4, m6, 11111110b
    pmaxub m6, m4

    mova m0, m6
    psubb m6, m7 ;;max - min
;; skip processing only if all blocks are below
;; the dering_threshold
    mova m1, [dering_threshold]
;; might need to mask off the high doubleword of m6
    pcmpged m1, m6
    ptest m1,m1
    jz .end
;; Some blocks might be below the dering threshold, but I imagine it won't
;; hurt to process them if it would there are ways around it    
    pavgb m7, m0
    ;;fill all of m7 with what is currently the low byte
    %if cpuflag(avx2)
    vpermq ymm7, ymm7, 0x00
    %endif
    punbcklbw mm7,mm7
    punbcklbw mm7,mm7
    punbcklbw mm7,mm7
    mova m0, [r1]
    mova m1, m0
    mova m2, m0
;; I'm not sure how to translate this since I'm not exactly sure what the code is doing
    

.end:
%if ARCH_X86_64 == 0
    add esp,(4 * %%stacksz)
%elif WIN64
    add, rsp,(4 * %%stacksz)
%endif    
    REP_RET
