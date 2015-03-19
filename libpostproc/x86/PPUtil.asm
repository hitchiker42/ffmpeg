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
%include "libavutil/x86/x86util.asm"
%include "PPContext.asm"
;; Macros to simplify moving packed data

;; copy low quadword to upper quadword(s)
;; no-op for mmx
%macro dup_low_quadword 1
%if cpuflag(sse2)
    pshufpd %1, %1, 0x00
%elif cpuflag(avx2)
    vpermq %1, %1, 0x00
%endif
%endmacro
;; copy low byte into all other bytes
%macro dup_low_byte 1-2
%if cpuflag(avx2)
;; avx shuffle instructions generally act on the lower 128 and upper 128 bits
;; independently, so we need to copy the lower 128 bits to the upper 128 bits
    vpermq %1,%1,0x00
%endif
%if cpuflag(ssse3)&& %0 == 2
    pxor %2,%2
    pshufb %1,%2
%else
    punpcklbw %1, %1
%if cpuflag(sse2)
    pshuflw %1,%1, 0x00
    pshufd %1,%2, 0x00
%else ;; mmx
    pshufw %1,%1, 0x00
%endif
%endif
%endmacro
;; move the low half of the mmx/xmm/ymm register in %2 into %1
;; %1 should be a memory location
%macro mov_vector_low_half 2
%if mmsize == 32
vextractf128 %1, %2, 0x00
%elif mmsize == 16
movlpd %1, %2
%elif mmsize == 8
movd %1, %2
%else
%error "mmsize defined to unsupported value"
%endif
%endmacro
;; move the high half of the mmx/xmm/ymm register in %2 into %1
;; %1 should be a memory location
%macro mov_vector_high_half 2-3 m0
%if mmsize == 32
vextractf128 %1, %2, 0x01
%elif mmsize == 16
movhpd %1, %2
%elif mmsize == 8
;; there's no instruction, pre sse4.1, to move the high 32 bits of an mmx
;; register, so use the optional third argument as a temporary register
;; shift it right by 32 bits and extract the low doubleword
movq %3, %2
psrl %3, 32
movd %1, %3
%else
%error "mmsize defined to unsupported value"
%endif
%endmacro

;; define packed conditional moves, of the form:
;; pcmovXXS dst, src, arg1, arg2, tmp
;; where XX is a comparision (eq,ne,gt,...) and S is a size(b,w,d,q)
;; copy src to dest, then compare arg1 with arg2 and store
;; the result in tmp, finally AND src with tmp.
%macro do_simd_sizes 2
%1 %2b
%1 %2w
%1 %2d
%1 %2q
%endmacro
;; macro generating macro
%macro gen_pcmovxx 1 
%macro pcmov%1 4-6 ,%1 ;;dst, src, cmp1, cmp2, [tmp = cmp2]
%if %0 == 5
%ifnidn %5,%3
    mova %5,%3
%endif
%endif
    pcmp%6 %5,%4
    mova %1, %2
    pand %1, %5
%endmacro
%endmacro
do_simd_sizes gen_pcmovxx,eq
do_simd_sizes gen_pcmovxx,ne
do_simd_sizes gen_pcmovxx,lt
do_simd_sizes gen_pcmovxx,le
do_simd_sizes gen_pcmovxx,gt
do_simd_sizes gen_pcmovxx,ge

;; Macros for defining simd constants
%macro define_qword_vector_constant 5
%if cpuflag(avx2)
SECTION_RODATA 32
%else
SECTION_RODATA 16
%endif
%1: 
    dq %2
%if cpuflag(sse2)
    dq %3
%if cpuflag(avx2)
    dq %4
    dq %5
%endif
%endif
SECTION_TEXT
%endmacro
;; Utility functions, maybe these should be macros for speed.

;; void duplicate(uint8 *src, int stride)
;; duplicate block_size pixels 5 times upwards
cglobal duplicate, 2, 2, 1
    neg r2
    mova m0, [r1]
    mova [r1 + r2 * 4], m0
    add r1, r2
    mova [r1], m0
    mova [r1 + r2], m0
    mova [r1 + r2 * 2], m0
    mova [r1 + r2 * 4], m0
    neg r2
    RET
;; void blockcopy(uint8_t *dst, int dst_stride, uint8_t *src, int src_stride,
;;                int level_fix, int64_t *packed_offset_and_scale)
;; Copy src to dst, and possibly fix the brightness
%macro gen_block_copy 0
cglobal block_copy, 6, 6, 8
    test r5, r5
    jz .simple
    mova m5, [r6] ;;offset
    mova m6, [r6 + 32] ;;scale
    lea r6, [r3 + r4] ;;dst + dst_stride
    lea r5, [r1 + r2] ;;src + src_stride
;; I don't know a ton about how to properly order instructions
;; to maximize pipelining, so this might not be optimial
%ifnmacro scaled_copy
%macro scaled_copy 4
    mova m0, %1
    mova m1, m0
    mova m2, %2
    mova m3, m2
%assign i 0
%rep 4
    punpcklbw m%i,m%i
    pmulhuw m%i, m6
    psubw m%i, m5
%assign i i+1
%endrep
    packuswb m0, m1
    packuswb m2, m3
    mova [%3], m0
    mova [%4], m2
%endmacro
%endif
    scaled_copy [r1], [r1 + r2], [r3], [r3 + r4]
    scaled_copy [r1 + r2*2], [r5 + r2*2], [r3 + r4*2], [r6 + r4*2]
    scaled_copy [r1 + r2*4], [r5 + r2*4], [r3 + r4*4], [r6 + r4*4]
    lea r5, [r5 + r2*4] 
    lea r6, [r6 + r3*4]
    scaled_copy [r5 + r2], [r5 + r2*2], [r6 + r4], [r6 + r4*2]
    jmp .end
.simple: ;;just a simple memcpy
    ;;if there's a better way to do this feel free to change it
    ;;Any necessary prefetching is done by the caller
    lea r5, [r1 + r2] ;;src + src_stride
    lea r6, [r5 + r2*4] ;;dst + dst_stride
    mova m0, [r1]
    mova m1, [r1 + r2]
    mova m2, [r1 + r2*2]
    mova m3, [r5 + r2*2]
    mova m4, [r1 + r2*4]
    mova m5, [r5 + r2*4]
    mova m6, [r6 + r2]
    mova m7, [r6 + r2*2]
    lea r5, [r3 + r4]
    lea r6, [r5 + r4*4]
    mova m0, [r3]
    mova [r3 + r4], m1
    mova [r3 + r4*2], m2
    mova [r5 + r4*2], m3
    mova [r3 + r4*4], m4
    mova [r5 + r4*4], m5
    mova [r6 + r4], m6
    mova [r6 + r4*2], m7
.end:
    REP_RET
%endmacro

;; Macros to emulate the ptest instruction for pre-sse41 cpus
;; Used to allow branching based on the values of simd registers
;; set zf if dst & src == 0
%macro ptest_neq 2-4 ;; dst, src, tmp1, tmp2 ; tmp1,2 are  general purpose registers
%if cpuflag(sse4)
    ptest %1, %2
%elif cpuflag(sse)
    pcmpeqb %1, %2
    pmovmskb %3, %1
    test %3, %3
%else ;;mmx
    pand %1, %2
    movd %3, %1
    psrlq %1, 32
    movd %4, %1
;; we just need to test if any of the bits are set so we can fold the
;; two 32 bit halves together
    or %3, %4
    test %3, %3
%endif
%endmacro
;; set cf if dst & ~src == 0 (i.e dst == src)
%macro ptest_eq 2-4 ;;dst, src, tmp1, tmp1 ;tmp1,2 are general purpose registers
%if cpuflag(sse4)
    ptest %1, %2
%elif cpuflag(sse)
    pcmpeqb %1, %2
    pmovmskb %3, %1
    neg %3 ;;sets cf if operand is non-zero
%else ;;mmx
    pand %1, %2
    movd %3, %1
    psrlq %1, 32
    movd %4, %1
    or %3, %4
    neg %3
%endif
%endmacro

;; cglobal transpose_block, 4, 6, 7 ;src, src_stride, dst, dst_stride
;;     lea r5, [r3 + 4*r4]
;; 
;;     mova  m0, [r1]
;;     mova  m1, [r1 + r2]
;;     mova  m2, m0
;;     punpcklbw  m0, m1
;;     punpckhbw  m2, m1
;; 
;;     mova  m1, [r1 + r2 * 3]
;;     mova  m3, [r1 + r2 * 4]
;;     mova  m4, m1
;;     punpcklbw  m1, m3
;;     punpckhbw  m4, m3
;; 
;;     mova  m3, m0
;;     punpcklwd  m0, m1
;;     punpckhwd  m3, m1
;;     mova  m1, m2
;;     punpcklwd  m2, m4
;;     punpckhwd  m1, m4
;; 
;;     mova [r3], m0
;;     mova [r3 + r4*2], m3
;;     mova [r3 + r4*4], m2
;;     mova [r5], m1
;; 
;; 
;;     mova  mr3, 64[r1]
;;     mova  m1, 80[r1]
;;     mova  m2, m0
;;     punpcklbw  mr3, m1
;;     punpckhbw  m2, m1
;; 
;;     mova  m1, 96[r1]
;;     mova  m3, r412[r1]
;;     mova  m4, m1
;;     punpcklbw  m1, m3
;;     punpckhbw  m4, m3
;; 
;;     mova  m3, m0
;;     punpcklwd  mr3, m1
;;     punpckhwd  m3, m1
;;     mova  m1, m2
;;     punpcklwd  m2, m4
;;     punpckhwd  m1, m4
;; 
;;     movd  4(0), m0
;;     psrlq  mr3, $32
;;     movd  4(r5), m0
;;     movd  4(r5, r4), m3
;;     psrlq  m3, $32
;;     movd  4(r5, r4, 2), m3
;;     movd  4(r3, r4, 4), m2
;;     psrlq  m2, $32
;;     movd  4(r6), m2
;;     movd  4(r6, r4), m1
;;     psrlq  m1, $32
;;     movd  4(r6, r4, 2), m1
;; RET
;; 
