/*
 * Copyright (c) 2015 Tucker DiNapoli
 *
 * Deinterlace functions
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "PPUtil.S"
function deInterlaceInterpolateLinear /* src, stride */
    mov r2, r0 @save initial src
@would a shift be faster or does it not matter
    add r3, r1, r1 @scale by 2 lines at a time
    @load v0-v4 from [r0], indexing by r3
    vldm128 0,4, r0, r3

    @     dst, src1, src2, tmp
    vavgb v5,  v0,   v1,   v9 @L1
    vavgb v6,  v1,   v2,   v9 @L3
    vavgb v7,  v2,   v3,   v9 @L5
    vavgb v8,  v3,   v4,   v9 @L7

    @store v5-v8 into [r2], indexing by r3
    vstm128 5, 8, r2, r3

    bx lr
endfunc

function deInterlaceInterpolateCubic /*src, stride*/
    mov r12, r1
    add r12, r1, lsl 1 @r4 = r1*3
    add r0, 12
    add r2, r0, r12
    add r3, r1, r1

    vldm128 0, 7, r0, r3 @{v0,v1...v6} = {L0,L2,...L12}

.ifndef deint_cubic
.macro deint_cubic a,b,d,e
@    vavgb v8, \a, \e, v15 @(a+e)/2
@    vavgb v9, \b, \d, v15 @(b+d)/2

@ average and expand bytes to words
    vavgl u8, v8, \a&_l, \e&_l
    vavgl u8, v9, \a&_h, \e&_h
    vavgl u8, v10, \b&_l, \d&_l
    vavgl u8, v11, \b&_h, \d&_h


    vsub.i16 v8, v10
    vsub.i16 v9, v11 @((a+e) - (b+d))/2
    vshr.i16 v8, #3
    vshr.i16 v9, #3 @((a+e) - (b+d))/16
    vadd.i16 v10, v8
    vadd.i16 v11, v9 @(9(b+d) - (a+e))/16
@pack back into bytes, saturating if necessary
    vqmovn.u16 v8_l, v10
    vqmovn.u16 v8_h, v11
    vst1.64 v8, [r2 :128], r3
.endm
.endif
    deint_cubic v0,v1,v2,v3 @L3
    deint_cubic v1,v2,v3,v4 @L5
    deint_cubic v2,v3,v4,v5 @L7
    deint_cubic v3,v4,v5,v6 @L9
    
    bx lr
endfunc
