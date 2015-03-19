;******************************************************************************
;*
;* Copyright (c) 2015 Tucker DiNapoli
;*
;* Utility code/marcos used in asm files for libpostproc
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
define_qword_vector_constant w04,0x0004000400040004,0x0004000400040004,\
                                 0x0004000400040004,0x0004000400040004
define_qword_vector_constant w05,0x0005000500050005,0x0005000500050005,\
                                 0x0005000500050005,0x0005000500050005
define_qword_vector_constant w20,0x0020002000200020,0x0020002000200020,\
                                 0x0020002000200020,0x0020002000200020

;; This uses the stack for temporary storage, the use of the stack
;; could probably be optimized somewhat
%macro gen_deblock 0
cglobal do_a_deblock, 5, 7, 7 ;src, step, stride, ppcontext, mode
    sub rsp, 176*mmsize
    lea r1, [r1 + r2*2]
    add r1, r2

    mov m7, [(r4 + PPContext.mmxDcOffset) + (r4 + PPContext.nonBQP) * 8]
    mov m6, [(r4 + PPContext.mmxDcThreshold) + (r4 + PPContext.nonBQP) * 8]

    lea r6, [r1 + r2]
    mova m0, [r1]
    mova m1, [r6]
    mova m3, m1
    mova m4, m1
    psubb m0, m1 ;; difference between 1st and 2nd block? row?
    paddb m0, m7
    pcmpgtb m0, m6

%ifnmacro get_mask_step
%macro get_mask_step 3
    mova %1, %3
    pmaxub m4, %1
    pminub m3, %1
    psubb %2, %1
    paddb %2, m7
    pcmpgtb %2, m6
    paddb m0, %2
%endmacro
    get_mask_step m2, m1, [r6 + r2]
    get_mask_step m1, m2, [r6 + r2*2]
    lea r6, [r6 + r2*4]
    get_mask_step m2, m1, [r1 + r2*4]
    get_mask_step m1, m2, [r6]
    get_mask_step m2, m1, [r6 + r2]
    get_mask_step m1, m2, [r6 + r2*2]
    get_mask_step m2, m1, [r1 + r2*8]

    mova m1, [r6 + r2*4]
    psubb m2, m1
    paddb m2, m7
    pcmpgtb m2, m6
    paddb m0, m2
    psubusb m4, m3
    
    pxor m6, m6
    mova m7, [r4 + PPContext.pQPb] ;QP, QP .... QP
    paddusb m7, m7 ;2QP, 2QP, ... 2QP
    paddusb m7, m4 ;diff >= 2QP -> 0
    pcmpeqb m7, m6 ;diff < 2QP -> 0
    pcmpeqb m7, m6 ;diff < 2QP -> 0, is this supposed to be here
    mova [rsp + 168*mmsize], m7; dc_mask
    
    mova m7, [r4 + PPContext.ppMode + PPMode.flatness_threshold]
    dup_low_byte m7
    psubb m6, m0
    pcmpgtb m6, m7
    mova [rsp + 160*mmsize], m6; eq_mask
    
    ptest m6, [rsp + 168*mmsize]
    ;; if eq_mask & dc_mask == 0 jump to .skip
    jnz .skip
    lea r6, [r2 * 8]
    neg r6 ;;r6 == offset
    mov r7, r1

    mova m0, [r4 + PPContext.pQPb]
    pxor m4, m4
    mova m6, [r1]
    mova m5, [r1+r2]
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
    
    mova m5, [r1 + r2 * 8]
    add r1, r2
    mova m7, [r1 + r2 * 8]
    mova m1, m5
    mova m2, m7

    psubusb m5, m7
    psubusb m1, m2
    por m2, m5
    mova m0, [r4 + PPContext.pQPb]
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
%defmacro pp_next 0
    mova m2, [r1]
    mova m3, [r1]
    add r1, r2
    punpcklbw m2, m4
    punpckhbw m3, m4
    paddw m0, m2
    paddw m1, m3
%endmacro
%endif
%ifnmacro pp_prev
%macro pp_prev 0
    mova m2, [r1]
    mova m3, [r1]
    add r1,r2
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
    mova [rsp + 8*mmsize], m1

    pp_next
    psubw m0, m5
    psubw m1, m6
    mova [rsp + 16*mmsize], m0
    mova [rsp + 24*mmsize], m1
    
    pp_next
    psubw m0, m5
    psubw m1, m6
    mova [rsp + 32*mmsize], m0
    mova [rsp + 40*mmsize], m1
    
    pp_next
    psubw m0, m5
    psubw m1, m6
    mova [rsp + 48*mmsize], m0
    mova [rsp + 56*mmsize], m1

    pp_next
    psubw m0, m5
    psubw m1, m6
    mova [rsp + 64*mmsize], m0
    mova [rsp + 72*mmsize], m1

    mova m6, m7
    punpckhbw m7, m4
    punpcklbw m6, m4

    pp_next
    mov r1, r7
    add r1, r2
    pp_prev
    mova [rsp + 80*mmsize], m0
    mova [rsp + 88*mmsize], m1

    pp_prev
    paddw m0, m6
    paddw m1, m7
    mova [rsp + 96*mmsize], m0
    mova [rsp + 104*mmsize], m1

    pp_prev
    paddw m0, m6
    paddw m1, m7
    mova [rsp + 112*mmsize], m0
    mova [rsp + 120*mmsize], m1

    pp_prev
    paddw m0, m6
    paddw m1, m7
    mova [rsp + 128*mmsize], m0
    mova [rsp + 136*mmsize], m1

    pp_prev
    paddw m0, m6
    paddw m1, m7
    mova [rsp + 144*mmsize], m0
    mova [rsp + 152*mmsize], m1

    mov r1, r7 ;; This has a fixme note in the C source, I'm not sure why
    add r1, r2
    
    mova m6, [rsp + 168*mmsize]
    pand m6, [rsp + 160*mmsize]
    pcmpeqb m5, m5 ;; m5 = 111...111
    pxor m6, m5 ;; aka. bitwise not m6
    pxor m7, m7
    mov r7, rsp

    sub r1, r6
.loop:
    mova m0, [r7]
    mova m1, [r7 + 8*mmsize]
    paddw m0, [r7 + 16*mmsize]
    paddw m1, [r7 + 24*mmsize]
    movva m2, [r1, r6]
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
    pand m4. m5
    por m0, m4
    mova m0, [r1 + r6]
    add r7, 16 
    add r6, r2  ;;offset += step
    js .loop
    jmp .test
.skip:
    add r1, r2

.test:
    pcmpeq m6, m6;;may change this register
    ptest m6, [rsp]
    jc .end

.end:
    REP_RET
%endmacro
