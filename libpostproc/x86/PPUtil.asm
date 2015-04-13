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
%if cpuflag(avx2)
    vpermq %1, %1, 0x00
%elif cpuflag(sse2)
    pshufd %1, %1, 00001010b
%endif
%endmacro
;; copy low byte into all other bytes
;; Optional argument is a temporary register to allow using pshufb
%macro dup_low_byte 1-2
%if cpuflag(avx2)
;; copy the lower 128 bits to the upper 128 bits, so avx shuffles work correctly
    vpermq %1,%1,0x00
%endif
;;would it be faster to use pshufb if the seond argument were in memory?
;;because we could store a 0 vector in memory for use in pshufb with 1 argument
%if cpuflag(ssse3) && %0 == 2
    pxor %2,%2
    pshufb %1,%2
%else
    punpcklbw %1, %1
%if cpuflag(sse2)
    pshuflw %1,%1, 0x00
    pshufd %1,%1, 0x00
%else ;; mmx
    pshufw %1,%1, 0x00
%endif
%endif
%endmacro

;; It may be useful to have macros to do the following:
;; fill each quadword with it's low byte
;; unpack the low 2-4 bytes into the low byte of each quadword


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

;; Macros for defining simd constants, 
;; Always defines a 256 bit, 32 byte aligned constant, which is more
;; size/alignment than is necessary for sse/mmx, but ensures the same
;; constant will work for all simd instruction sets
%macro define_vector_constant 5
%xdefine %%section __SECT__
SECTION_RODATA 32
%1:
    dq %2
    dq %3
    dq %4
    dq %5
%%section
%endmacro
;; convenience macro to define a simd constant where each quadword is the same
%macro define_vector_constant 2
    define_vector_constant %1,%2,%2,%2,%2
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

;;make pshufw work with xmm/ymm registers, via shuffling
;;the low and high words seperately
%macro pshufw 3
%if cpuflag(sse2) | cpuflag(avx2)
    pshuflw %1,%2,%3
    pshufhw %1,%2,%3
%else
    pshufw %1,%2,%3
%endif
%endmacro
;;find the minimum/maixum byte in a simd register
;;the phsufw's can/should probably be changed for
;;sse/avx since it's two instructions  
%macro horiz_min_max_ub 2-3 ;;src, tmp, op
    mova %2, %1
    psrlq %1, 8
    %3 %1, %2
    pshufw %2, %1, 0b11111001
    %3 %1,%2
    pshufw %2, %1, 0b11111110
    %3 %1, %2
%endmacro
%macro phminub 2
    horiz_min_max_ub %1,%2,pminub
%endmacro
%macro phmaxub 2
    horiz_min_max_ub %1,%2,pmaxub
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



define_vector_constant b01, 0x0101010101010101
define_vector_constant b02, 0x0202020202020202
define_vector_constant b08, 0x0808080808080808
define_vector_constant b80, 0x8080808080808080
define_vector_constant w04, 0x0004000400040004
define_vector_constant w05, 0x0005000500050005
define_vector_constant w20, 0x0020002000200020
define_vector_constant q20, 0x0000000000000020 ;;dering threshold
%xdefine packed_dering_threshold q20
%assign dering_threshold 0x20
