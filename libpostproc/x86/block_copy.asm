;******************************************************************************
;*
;* Copyright (c) 2015 Tucker DiNapoli
;*
;* Code to copy, and optionally scale blocks
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
;; void duplicate(uint8 *src, int stride)
;; duplicate block_size pixels 5 times upwards
;; this should probably ultimately be inlined
%macro gen_duplicate 0
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
%endmacro
;; void blockcopy(uint8_t *dst, int dst_stride, uint8_t *src, int src_stride,
;;                int level_fix, int64_t *packed_offset_and_scale)
;; Copy src to dst, and possibly fix the brightness
%macro gen_block_copy 0
cglobal blockCopy, 6, 6, 8
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
    punpcklbw m %+ i,m %+ i
    pmulhuw m %+ i, m6
    psubw m %+ i, m5
%assign i i+1
%endrep
    packuswb m0, m1
    packuswb m2, m3
    mova %3, m0
    mova %4, m2
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

INIT_MMX mmx2
gen_duplicate
gen_block_copy

INIT_XMM sse2
gen_duplicate
gen_block_copy

INIT_YMM avx2
gen_duplicate
gen_block_copy
