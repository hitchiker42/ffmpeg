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
define_vector_constant low_byte_mask, 0x00000000000000ff
;;when anded with a register converts every other byte into a word
;;define_vector_constant word_mask, 0x00ff00ff00ff00ff
;;I assume it's faster to generate it as is done it the code then to load it
%macro gen_dering 0
cglobal dering, 3, 6, 7, 3*mmsize;src, stride, context
    lea r3, [r0 + r1]
    lea r4, [r3 + r1 * 4]
    pxor m6, m6 ;;all bits 0
    pcmpebq m7, m7 ;;all bits 1
    mova m0, [r2 + PPContext.pQPb] ;;QP
%if cpuflag(sse2)
    cmpeqb m5, m5
    punpcklbw m5, m6 ;;a register w/each word = 0x00ff
    pand m0, m1 ;;expand QP to words
%else ;;mmx
    punpcklbw m0, m6
%endif
    psrlw m0, 1
    psubw m0, m7
    packuswb m0, m0
    mova [r2 + PPContext.pQPb2], m0
;;compute the minimum and maximum byte in each column of the
;;source and store them in m7,m6 respectively
%ifnmacro find_min_max
%macro find_min_max 1
    mova m0, %1
    pminub m7, m0
    pmaxub m6, m0
%endmacro
%endif
    find_min_max [r3] ;;L1
    find_min_max [r3 + r1] ;;L2
    find_min_max [r3 + r1 * 2] ;;L3
    find_min_max [r0 + r1 * 4] ;;L4
    find_min_max [r4] ;;L5
    find_min_max [r4 + r1] ;;L6
    find_min_max [r4 + r1 * 2] ;;L7
    find_min_max [r0 + r1 * 8] ;;L8

    phminub m7, m4
    phmaxub m6, m4
    mova m0, m6
    psubb m6, m7
%if cpuflag(sse)
;;if we're processing more than one block
;;test w/dering_threshold for each block
;;and only skip dering if all blocks fail
    mova m4, [packed_dering_threshold]
    mova m5, [low_byte_mask]
    pand m6, m5
    pcmpgt m6, m4
    movmaskb r5, m6
    test r5, r5
    jz .end
%else ;;mmx
    movd r5, m6
    mov r6, dering_threshold
    test r5b, r6b
    jb .end
%endif
    dup_low_byte m7, m0
    mova [rsp], m7
    mova m0, [r0] ;;L10
    mova m1, m0
    mova m2, m0
    psllq m1, 8
    psrlq m2, 8
;;these offsets might need to be adjusted
    mova m3, [r0 + -4]
    mova m4, [r0 + 8]
    psrlq m3, 24
    psllq m4, 56
    por m1, m3 ;;L00
    por m2, m4 ;;L20
    ;;the calculaton of L00,L20 may need to be adjusted,
    ;;but everything below shouldn't be effected
    mova m3, m1 ;;L00
    
.end
    REP_RET
