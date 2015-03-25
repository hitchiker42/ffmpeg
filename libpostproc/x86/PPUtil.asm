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
;; convience macro to define a simd constant where each quadword is the same
%macro define_qword_vector_constant 2
    define_qword_vector_constant %1,%2,%2,%2,%2
%endmacro
;; Utility functions, maybe these should be macros for speed.

;; void duplicate(uint8 *src, int stride)
;; duplicate block_size pixels 5 times upwards
cglobal duplicate, 2, 2, 1
    neg r1
    mova m0, [r0]
    mova [r0 + r1 * 4], m0
    add r0, r1
    mova [r0], m0
    mova [r0 + r1], m0
    mova [r0 + r1 * 2], m0
    mova [r0 + r1 * 4], m0
    neg r1
    RET
;; void blockcopy(uint8_t *dst, int dst_stride, uint8_t *src, int src_stride,
;;                int level_fix, int64_t *packed_offset_and_scale)
;; Copy src to dst, and possibly fix the brightness
%macro gen_block_copy 0
cglobal block_copy, 6, 6, 8
    test r4, r4
    jz .simple
    mova m5, [r5] ;;offset
    mova m6, [r5 + 32] ;;scale
    lea r5, [r2 + r3] ;;dst + dst_stride
    lea r4, [r0 + r1] ;;src + src_stride
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
    scaled_copy [r0], [r0 + r1], [r2], [r2 + r3]
    scaled_copy [r0 + r1*2], [r4 + r1*2], [r2 + r3*2], [r5 + r3*2]
    scaled_copy [r0 + r1*4], [r4 + r1*4], [r2 + r3*4], [r5 + r3*4]
    lea r4, [r4 + r1*4]
    lea r5, [r5 + r2*4]
    scaled_copy [r4 + r1], [r4 + r1*2], [r5 + r3], [r5 + r3*2]
    jmp .end
.simple: ;;just a simple memcpy
    ;;if there's a better way to do this feel free to change it
    ;;Any necessary prefetching is done by the caller
    lea r4, [r0 + r1] ;;src + src_stride
    lea r5, [r4 + r1*4] ;;dst + dst_stride
    mova m0, [r0]
    mova m1, [r0 + r1]
    mova m2, [r0 + r1*2]
    mova m3, [r4 + r1*2]
    mova m4, [r0 + r1*4]
    mova m5, [r4 + r1*4]
    mova m6, [r5 + r1]
    mova m7, [r5 + r1*2]
    lea r4, [r2 + r3]
    lea r5, [r4 + r3*4]
    mova m0, [r2]
    mova [r2 + r3], m1
    mova [r2 + r3*2], m2
    mova [r4 + r3*2], m3
    mova [r2 + r3*4], m4
    mova [r4 + r3*4], m5
    mova [r5 + r3], m6
    mova [r5 + r3*2], m7
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

;; transpose 8x8 matrices of bytes

define_qword_vector_constant low_mask, 0x00000000ffffffff
define_qword_vector_constant high_mask, 0xffffffff00000000
%macro transpose 0 ;src, src-stride, tmp1, tmp2
%if cpuflag(sse4)
    %assign stack_size 4*mmsize
%else
    %assign stack_size 8*mmsize
cglobal transpose, 4, 5, 8, stack_size ;src,src-stride, dst,dst-stride
    lea r4, [r0 + r1]
    mova m0, [r0] ;1
    mova m1, [r4] ;2
    mova m2, m0 ;1
    mova m4, [r0 + 2*r1] ;3
    mova m3, [r4 + 2*r1] ;4
    mova m5, m4 ;3
    lea r0 [r0 + 4*r1]
;; ij is the j'th element of the i'th row
;; the comments only show one block, there are multiple
;; blocks in one register for sse and avx code


;; Maybe make this a macro? I only do it twice though
    punpcklbw m0, m1 ;24,14,23,13,22,12,21,11 = 1,2 L
    punpckhbw m2, m1 ;28,18,27,17,26,16,25,15 = 1,2 H
    punpcklbw m4, m3 ;3,4 L
    punpckhbw m5, m3 ;3,4 H
    mova m1, m0
    mova m3 ,m2

    punpcklwd m0, m3 ;42,32,22,12,41,31,21,11
    punpckhwd m1, m3 ;44,34,24,14,43,33,23,13

    punpcklwd m2, m5 ;46,36,26,16,45,35,25,15
    punpckhwd m3, m5 ;48,38,28,18,47,37,27,17

    ;; We have the 1st 4 bytes of each transposed row
%assign %%i 0
%assign %%j 0
%rep 4
;; could be movd for mmx
    mova [rsp + %%i*mmsize], m%%j
%if !cpuflag(sse4)
%assign %%i %%i+1
    psrlq m%%j, 32
    mova [rsp + %%i*mmsize], m%%j
%endif
%assign %%i %%i+1
%assign %%j %%j+1
%endrep
    lea r4, [r0 + r1]
    mova m0, [r0] ;5
    mova m1, [r4] ;6
    mova m2, m0 ;5
    mova m4, [r0 + 2*r1] ;7
    mova m3, [r4 + 2*r1] ;8
    mova m5, m4 ;7

    punpcklbw m0, m1 ;64,54,63,53,62,52,61,51 = 5,6 L
    punpckhbw m2, m1 ;68,58,67,57,66,56,65,55 = 5,6 H
    punpcklbw m4, m3 ;7,8 L
    punpckhbw m5, m3 ;7,8 H
    mova m1, m0
    mova m3, m2

    punpcklwd m0, m4 ;82,72,62,52,81,71,61,51
    punpckhwd m1, m4 ;84,74,64,54,83,73,63,53

    punpcklwd m2, m5 ;86,76,66,56,85,75,65,55
    punpckhwd m3, m5 ;88,78,68,58,87,77,67,57

;; m4-m7 are free
    lea r0, [r2 + r3]
    lea r1, [r0 + 4*r3]
%if !cpuflag(sse4) && cpuflag(sse2)
    mova m6, [low_mask]
    mova m7, [high_mask]
%endif
%ifnmacro store_results
    ;; only a macro to make it possible to loop over the destination locations
%macro store_results 8
%assign %%i 0
%rep 4
;; could possibly use vmaskmov if we have avx2
%if cpuflag(sse4)
    mova m4, m%%i
    psllq m4, 32 ;81,71,61,51,0,0,0,0
    pblendw m4, [rsp+%%i*mmsize],0b00110011 ;81,71,61,51,41,31,21,11
    mova [%1],m4
;; avx supports unaligned memory references, I don't
;; know if this will be faster though
%if cpuflag(avx)
    pblendw m%%i, [rsp + 4 + %%i*mmsize], 0b00110011 ;;82,72,...
%else
    mova m4, [rsp + %%i*mmsize]
    psrlq m4, 32
    pblendw m%%i, m4, 0x00110011
%endif
    mova [%2], m%%i

%assign %%i %%i+1
%else
%if cpuflag(sse2)
    mova m4, m%%i
    psllq m4, 32 ;81,71,61,51,0,0,0,0
    mova m5, [rsp + %%i*mmsize] ;42,32,22,21,41,31,21,11
    pand m%%i, m7 ;82,72,62,52,0,0,0,0
    pand m5, m6 ;0,0,0,0,41,31,21,11
    por m4, m5
    mova [%1],m4
    mova m5, [rsp + (%%i+i)*mmsize];0,0,0,0,42,32,22,12
    por m5, m%%i
    mova [%2], m5
%else ;;mmx
    mov r4, [rsp + %%i*mmsize]
    movd [%1 + 4], m%%i
    mov [%1], r4
    psrlq m%%i, 32
    mov r4, [rsp + (%%i+1)*mmsize]
    movd [%2 + 4], m%%i
    mov r4, [%2]
%endif ;;cpuflag(sse2)
%assign %%i %%i+2
%endif ;;cpuflag(sse4)
%rotate 2
%endrep
%endmacro ;;store results
%endif ;;ifnmacro
    store_results (r2),(r2+r3),(r2+2*r3),(r0+2*r3),\
    (r2+4*r3),(r1),(r1+r3),(r1+2*r3)
RET
%endmacro
