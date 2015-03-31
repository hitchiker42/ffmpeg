/*
 * Copyright (c) 2015 Tucker DiNapoli
 *
 * Utility code/marcos used in asm files for libpostproc
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
e * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "libavutil/arm/asm.S"
/*(a+b)/2 == ((a&b) + (a^b)/2)*/

.macro vavg type, dst, src1, src2, tmp
    veor.\type  \tmp, \src1, \src2
    vand.\type  \dst, \src1, \src2
    vshra.\type \dst, \tmp, #1
.endm

.macro vavgb dst, src1, src2, tmp
    vavg u8, \dst, \src1, \src2, \tmp
.endm

.macro alias name, reg
    \name .req reg
.endm
.macro aliasq name, reg, type
    \name .qn \reg\().\type
.endm
.macro aliasd name, reg, type
    \name .dn \reg\().\type
.endm
.macro unalias name
   .unreq \name
.endm

.macro struc name
    .struct 0
\name\():
.endm
.macro field name, sz
\name\():
    .struct \name + sz
.endm
.macro endstruc section=".text"
    .section \section
.endm
.altmacro    
 @ define QN_l to DN*2 and QN_h to DN*2+1
.macro alias_doubleword_to_quadword cnt
    LOCAL loop
    .macro loop i, j, k
      alias q\i&&_l, d\j
      alias q\i&&_h, d\k
    .if \i < \cnt
      loop %(\i+1), %(\j+2), %(\k+2)
    .endif
    .endm
    loop 0,0,1
.endm
/* load num quadword registers, starting at base and indexing by scale*/
/* loads into registers starting at Q\start*/
.macro vldm128 start, stop, base, scale
    LOCAL loop
    .macro loop i
        vld1.64 q\i, [\base :128], \scale
        .if \i+1 <= \stop
           loop %(\i+1)
        .endif
    .endm
    loop \start
.endm
.macro vstm128 start, stop, base, scale
    LOCAL loop
    .macro loop i
        vst1.64 q\i, [\base :128], \scale
        .if \i+1 <= \stop
           loop %(\i+1)
        .endif
    .endm
    loop \start
.endm


alias_doubleword_to_quadword 15
