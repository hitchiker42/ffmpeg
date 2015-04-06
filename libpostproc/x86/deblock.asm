;******************************************************************************
;*
;* Copyright (c) 2015 Tucker DiNapoli (T.DiNapoli42 at gmail.com)
;*
;* deblock filter 
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
define_qword_vector_constant w04,0x0004000400040004
define_qword_vector_constant w05,0x0005000500050005
define_qword_vector_constant w20,0x0020002000200020
SECTION .text
%macro gen_deblock 0
;; This is a version of do_a_deblock that should work for mmx,sse and avx
;; on x86 and x85_64.

cglobal do_a_deblock, 5, 6, 7, 22 * mmsize ;src, step, stride, ppcontext, mode
;; stride, mode arguments are unused, but kept for compatability with
;; existing c version. They will be removed eventually
    lea r0, [r0 + r1*2]
    add r0, r1

    mova m7, [(r3 + PPContext.mmx_dc_offset) + (r3 + PPContext.nonBQP) * 8]
    mova m6, [(r3 + PPContext.mmx_dc_threshold) + (r3 + PPContext.nonBQP) * 8]

    lea r5, [r0 + r1]
    mova m0, [r0]
    mova m1, [r5]
    mova m3, m1
    mova m4, m1
    psubb m0, m1 ;; difference between 1st and 2nd line
    paddb m0, m7
    pcmpgtb m0, m6

%ifnmacro get_mask_r1
%macro get_mask_r1 3
    mova %1, %3
    pmaxub m4, %1
    pminub m3, %1
    psubb %2, %1
    paddb %2, m7
    pcmpgtb %2, m6
    paddb m0, %2
%endmacro
%endif
    get_mask_r1 m2, m1, [r5 + r1]
    get_mask_r1 m1, m2, [r5 + r1*2]
    lea r5, [r5 + r1*4]
    get_mask_r1 m2, m1, [r0 + r1*4]
    get_mask_r1 m1, m2, [r5]
    get_mask_r1 m2, m1, [r5 + r1]
    get_mask_r1 m1, m2, [r5 + r1*2]
    get_mask_r1 m2, m1, [r0 + r1*8]

    mova m1, [r5 + r1*4]
    psubb m2, m1
    paddb m2, m7
    pcmpgtb m2, m6
    paddb m0, m2
    psubusb m4, m3

    pxor m6, m6
    mova m7, [r3 + PPContext.pQPb] ;QP, QP .... QP
    paddusb m7, m7 ;2QP, 2QP, ... 2QP
    paddusb m7, m4 ;diff >= 2QP -> 0
    pcmpeqb m7, m6 ;diff < 2QP -> 0
    pcmpeqb m7, m6 ;diff < 2QP -> 0, is this supposed to be here
    mova [rsp + 21*mmsize], m7; dc_mask

    mova m7, [r3 + PPContext.ppMode + PPMode.flatness_threshold]
    dup_low_byte m7, m6
%if cpuflag(ssse3)
    pxor m6,m6
%endif
    psubb m6, m0
    pcmpgtb m6, m7
    mova [rsp + 20*mmsize], m6; eq_mask

    ptest_neq m6, [rsp + 21*mmsize], r5, r6

    ;; if eq_mask & dc_mask == 0 jump to .skip
    jz .skip
    lea r5, [r1 * 8]
    neg r5 ;;r5 == offset
    mov r6, r0

    mova m0, [r3 + PPContext.pQPb]
    pxor m4, m4
    mova m6, [r0]
    mova m5, [r0+r1]
    mova m1, m5
    mova m2, m6

    psubusb m5, m6
    psubusb m2, m1
    por m2, m5 ;;abs diff of lines
    psubusb m0, m2 ;;diff >= QP -> 0s
    pcmpeqb m0, m4 ;;dif >= QP -> 1s

    pxor m1, m6
    pand m1, m0
    pxor m6, m1

    mova m5, [r0 + r1 * 8]
    add r0, r1
    mova m7, [r0 + r1 * 8]
    mova m1, m5
    mova m2, m7

    psubusb m5, m7
    psubusb m1, m2
    por m2, m5
    mova m0, [r3 + PPContext.pQPb]
    psubusb m0, m2
    pcmpeqb m0, m4

    pxor m1, m7
    pand m1, m0
    pxor m7, m1

    mova m5, m6
    punpckhbw m6, m4
    punpcklbw m5, m4

    mova m0, m5
    mova m1, m6
    psllw m0, 2
    psllw m1, 2
    paddw m0, [w04]
    paddw m1, [w04]
%ifnmacro pp_next
%macro pp_next 0
    mova m2, [r0]
    mova m3, [r0]
    add r0, r1
    punpcklbw m2, m4
    punpckhbw m3, m4
    paddw m0, m2
    paddw m1, m3
%endmacro
%endif
%ifnmacro pp_prev
%macro pp_prev 0
    mova m2, [r0]
    mova m3, [r0]
    add r0,r1
    punpcklbw m2, m4
    punpckhbw m3, m4
    psubw m0, m2
    psubw m1, m3
%endmacro
%endif
    pp_next
    pp_next
    pp_next
    mova [rsp], m0
    mova [rsp + 1*mmsize], m1
%rep 4
%assign %%i 2
    pp_next
    psubw m0, m5
    psubw m1, m6
    mova [rsp + (%%i)*mmsize], m0
    mova [rsp + (%%i+1)*mmsize], m1
%assign %%i %%i+2
%endrep

    mova m6, m7
    punpckhbw m7, m4
    punpcklbw m6, m4

    pp_next
    mov r0, r6
    add r0, r1
    pp_prev
    mova [rsp + 10*mmsize], m0
    mova [rsp + 11*mmsize], m1
%rep 4
%assign %%i 12
    pp_prev
    paddw m0, m6
    paddw m1, m7
    mova [rsp + (%%i)*mmsize], m0
    mova [rsp + (%%i+1)*mmsize], m1
%assign %%i %%i+2
%endrep

    mov r0, r6 ;; This has a fixme note in the C source, I'm not sure why
    add r0, r1

    mova m6, [rsp + 21*mmsize]
    pand m6, [rsp + 20*mmsize]
    pcmpeqb m5, m5 ;; m5 = 111...111
    pxor m6, m5 ;; aka. bitwise not m6
    pxor m7, m7
    mov r6, rsp

    sub r0, r5
.loop:
    mova m0, [r6]
    mova m1, [r6 + 1*mmsize]
    paddw m0, [r6 + 2*mmsize]
    paddw m1, [r6 + 3*mmsize]
    mova m2, [r0 + r5]
    mova m3, m2
    mova m4, m2
    punpcklbw m2, m7
    punpckhbw m3, m7
    paddw m0, m2
    paddw m1, m3
    paddw m0, m2
    paddw m1, m3
    psrlw m0, 4
    psrlw m1, 4
    packuswb m0, m1
    pand m0, m6
    pand m4, m5
    por m0, m4
    mova m0, [r0 + r5]
    add r6, 16
    add r5, r1  ;;offset += r1
    js .loop
    jmp .test
.skip:
    add r0, r1

.test:
;; if eq_mask is all 1s jump to the end
    pcmpeqb m6, m6
    ptest_eq m6, [rsp + 20*mmsize], r5, r6
    jc .end

    mov r6, r0
    pxor m7, m7
    mova m0, [r0]
    mova m1, m0
    punpcklbw m0, m7 ;low part of line 0, as words
    punpckhbw m1, m7 ;high ''                    ''

    mova m2, [r6 + r1]
    lea r5, [r6 + r1*2]
    mova m3, m2
    punpcklbw m2, m7 ;line 1, low
    punpckhbw m3, m7 ;line 1, high

    mova m4, [r5]
    mova m5, m4
    punpcklbw m4, m7 ; line 2, low
    punpckhbw m5, m7 ; line 2, high

    ;; get ready for lots of math
    ;; LN = low bytes of row N, as words
    ;; HN = high bytes of row N, as words

;; TODO: try to write a macro to simplifiy this next block of code

    paddw m0, m0 ;;2L0
    paddw m1, m1 ;;2H0
    psubw m2, m4 ;;L1 - L2
    psubw m3, m5 ;;H1 - H2
    psubw m0, m2 ;;2L0 - L1 + L2
    psubw m1, m3 ;;2H0 - H1 + H2

    psllw m2, 2 ;4(L1-L2)
    psllw m3, 2 ;4(H1-H2)
    psubw m0, m2 ; 2L0 - 5L1 + 5L2
    psubw m1, m3 ; 2H0 - 5H1 + 5H2

    mova m2, [r5 + r1]
    mova m3, m2
    punpcklbw m2, m7 ; L3
    punpckhbw m3, m7 ; H3

    psubw m0, m2
    psubw m1, m3
    psubw m0, m2 ;; 2L0 - 5L1 - 5L2 - 2L3
    psubw m1, m3 ;; high is the same, unless explicitly stated

;; TODO: replace stack use here with extra registers for sse/avx
    mova [rsp], m0
    mova [rsp + 1*mmsize], m1

    mova m0, [r5 + r1*2]
    mova m1, m0
    punpcklbw m0, m7 ; L4
    punpckhbw m1, m7 ; H4

    psubw m2, m0 ;L3-L4
    psubw m3, m1
    mova [rsp + 2*mmsize], m2
    mova [rsp + 3*mmsize], m3
    paddw m4, m4 ;2L2
    paddw m5, m5
    psubw m4, m2 ;2L2 - L3 + L4
    psubw m5, m3

    lea r6, [r5 + r1]
    psllw m2, 2 ;4(L3-L4)
    psllw m3, 2
    psubw m4, m2 ;2L2 - 5L3 + 5L4
    psubw m5, m3

    mova m2, [r6 + r1*2]
    mova m3, m2
    punpcklbw m2, m7
    punpckhbw m3, m7
    psubw m4, m2
    psubw m5, m3
    psubw m4, m2  ;;2L2 - 5L3 + 5L4 - 2L5
    psubw m5, m3
;; Use extra registers here
    mova m6, [r5 + r1*4]
    punpcklbw m6, m7 ;; L6
    psubw m2, m6 ;;L5 - L6
    mova m6, [r5 + r1*4]
    punpcklbw m6, m7 ;; H6
    psubw m3, m6 ;;H5 - H6

    paddw m0, m0 ;;2L4
    paddw m1, m1
    psubw m0, m2 ;;2L4 - L5 + L6
    psubw m1, m3

    psllw m2, 2 ;4(L5-L6)
    psllw m3, 2
    psubw m0, m2 ;;2L4- 5L5 + 5L6
    psubw m1, m2

    mova m2, [r6 + r1*4]
    mova m3, m2
    punpcklbw m2, m7 ;;L7
    punpcklbw m3, m7

    paddw m2, m2 ;;2L7
    paddw m3, m3
    psubw m0, m2 ;;2L4 - 5L5 - 5L6 + 2L7
    psubw m1, m3

    mova m2, [rsp]
    mova m3, [rsp + 1*mmsize]
;; Use extra  regs
    mova m6, m7 ;;pxor m6, m6
    psubw m6, m0
    pmaxsw m0, m6 ;;|2L4 - 5L5 + 5L6 - 2L7|

    mova m6, m7
    psubw m6, m1
    pmaxsw m1, m6

    mova m6, m7
    psubw m6, m2
    pmaxsw m2, m6 ;;|2L0 - 5L1 + 5L2 - 2L3|

    mova m6, m7
    psubw m6, m3
    pmaxsw m3, m6

    pminsw m0, m2 ;;min(|2L4 - 5L5  + 5L6 - 2L7|,|2L0 - 5L1 + 5L2 - 2L3|)
    pminsw m1, m3

    mova m2, [r3 + PPContext.pQPb]
    punpcklbw m2, m7
;; Maybe use pmovmskb here, to get signs
    mova m6, m7
    pcmpgtw m6, m4 ;;sgn(2L2 - 5L3 - 5L4 - 2L5)
    ;; next 2 instructions take the 2s complement of the negitive values in m4
    pxor m4, m6
    psubw m4, m6 ;;|2L2 -5L3 -5L4 -2L5|
    pcmpgtw m7, m5
    pxor m5, m7
    psubw m5, m7

    psllw m2, 3 ;; 8QP
    mova m3, m2
;; zero the words in m2,m3 that are less than QP
    pcmpgtw m2, m4
    pcmpgtw m3, m5
    pand m4, m2
    pand m5, m3

    psubusw m4, m0
    psubusw m5, m1


    mova m2, [w05]
    pmullw m4, m2
    pmullw m5, m2
    mova m2, [w20]
    paddw m4, m2
    paddw m5, m2
    psrlw m4, 6
    psrlw m5, 6

    mova m0,[rsp + 2*mmsize];;L3-L4
    mova m1,[rsp + 3*mmsize]

    pxor m2, m2
    pxor m3, m3

    pcmpgtw m2, m0 ;;sgn(L3-L4)
    pcmpgtw m3, m1
    pxor m0, m2
    pxor m1, m3
    psubw m0, m2
    psubw m1, m3
    psrlw m0, 1 ; |L3-L4|/2
    psrlw m1, 1

    pxor m6, m2
    pxor m7, m2
    pand m4, m2
    pand m5, m3

    pminsw m4, m0
    pminsw m5, m1

    pxor m4, m6
    pxor m5, m7
    psubw m4, m6
    psubw m4, m6
    psubw m5, m7
    packsswb m5, m4 ;;back to bytes
    mova m1, [rsp + 20*mmsize]
    pandn m1, m4
    mova m0, [r6]
    paddb m0, m1
    mova [r6], m0,
    mova m0, [r6 + r1]
    psubb m0, m1
    mova [r6 + r1], m0

.end:
    add rsp, [rsp + 22*mmsize] ;;undo alignment
    add rsp, (22*mmsize)+gprsize
    REP_RET
%endmacro

INIT_MMX mmx2
gen_deblock
INIT_XMM sse2
gen_deblock
INIT_YMM avx2
gen_deblock
