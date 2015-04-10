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
@ used to get the size/signedness of a type, for use in macros
@ necessary since gnu as doesn't have any means of getting a substring
.set u8_size 8
.set u8_sign u
.set s8_size 8
.set s8_sign s
.set u16_size 16
.set u16_sign u
.set s16_size 16
.set s16_sign s
.set u32_size 32
.set u32_sign u
.set s32_size 32
.set s32_sign s
.set u64_size 64
.set u64_sign u
.set s64_size 64
.set s64_sign s
/*(a+b)/2 == ((a&b) + (a^b)/2)*/
.macro vavg type, dst, src1, src2, tmp
    veor.\type  \tmp, \src1, \src2
    vand.\type  \dst, \src1, \src2
    vrshra.\type \dst, \tmp, #1
.endm
/*
.macro vavg type, dst, src1, src2, tmp
    vaddl.\type \tmp, \src1&_l, \src2&_l
@this type needs to be 2x the original type (use a table or something)
    vrshrn.\type&_sign%(\type&_size * 2) \dst&_l, \tmp, 1
    vaddl.\type \tmp, \src1&_h, \src2&_h
    vrshrn.\type&_sign%(\type&_size &2) \dst&_h, \tmp, 1
.endm
*/
.macro vavgb dst, src1, src2, tmp
    vavg u8, \dst, \src1, \src2, \tmp
.endm

@ average the elements from two doublewords and store the results into
@ a quadword, before averaging sign/zero extend the elements
@ This is faster than averaging then expanding as two seperate operations
.macro vavgl type, dst, src1, src2
    vaddl.\type \dst, \src1, \src2
@ rounding shift right (this should emulate the results of pavgb on x86
    vrshr.\type \dst, 1
.endm
@ shorthand for vavgl u8, dst, src1, src2
.macro vavglb dst, src1, src2
    vavgl u8, \dst, \src1, \src2
.endm

.macro alias name, reg
    \name .req reg
.endm
.macro aliasq name, reg, type
.ifnb type
    \name .qn \reg\().\type
.else
    \name .qn \reg
.endif
.endm
.macro aliasd name, reg, type
.ifnb type
    \name .dn \reg\().\type
.else
    \name .dn \reg
.endif
.endm
.macro unalias name
   .unreq \name
.endm
.altmacro
@ these macros allow defining the layout of a structure, such that the
@ fields can be accessed by adding the name of the field to the base
@ of the structure in asm code
.macro struc name
    .struct 0
\name&:
.endm
.macro field name, sz, align
.ifnb \align
    .struct . + ((\align - (. % \align)) % \align)
.endif
\name&:
    .struct \name + \sz
.endm
.endmacro
    
.macro endstruc name, section=".text"
\name&_size:
    .section \section
.endm
@ define QN_l to DN*2 and QN_h to DN*2+1
@ not really useful anymore
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
.macro unalias_all
    LOCAL loop
    .macro loop i,j
      .unreq r&\i
      .unreq q&\i
      .unreq d&\i
      .unreq d&\j
    .if \i < \15
      loop %(\i+1) %(\j+1)
    .endif
    .endm
    loop 0, 16
.endm
@ load num quadword registers, starting at base and indexing by scale
@ loads into registers starting at v\start
.macro vldm128 start, stop, base, scale
    LOCAL loop
    .macro loop i
        vld1.64 v\i, [\base :128], \scale
        .if \i+1 <= \stop
           loop %(\i+1)
        .endif
    .endm
    loop \start
.endm
.macro vstm128 start, stop, base, scale
    LOCAL loop
    .macro loop i
        vst1.64 v\i, [\base :128], \scale
        .if \i+1 <= \stop
           loop %(\i+1)
        .endif
    .endm
    loop \start
.endm
@ To allow permuting register names we use vN as the qword register name
@ and vN_l/vN_h as the equivlent dword register names (at least before swapping)
@ create typed aliases, i.e:
@ vN_uM = qN.uM/vN_sM = qN.sM/Vn_uM_l = d2N.uN, etc M=8,16,32,64
.macro set_sign_alias 
.endm
.macro set_sign_aliases v,q
    LOCAL do_alias
    .macro do_alias i,j,k,l,n
        aliasq v\l&_u\n, q\i, u\n
        aliasq v\l&_s\n, q\i, s\n
        aliasq v\l&_u\n&_l, d\j, u\n
        aliasq v\l&_u\n&_h, d\k, u\n
        aliasq v\l&_s\n&_l, d\j, s\n
        aliasq v\l&_s\n&_h, d\k, s\n
    .endm
    do_alias \q, %(\q*2), %(\q*2+1), \v, 8
    do_alias \q, %(\q*2), %(\q*2+1), \v, 16
    do_alias \q, %(\q*2), %(\q*2+1), \v, 32
    do_alias \q, %(\q*2), %(\q*2+1), \v, 64
.endm
    
.macro set_v_alias v, q
    LOCAL do_alias
    .macro do_alias i,j,k,n
        unalias v\n
        unalias v\n&&_1
        unalias v\n&&_h
        alias v\n q\i
        alias v\n&&_l d\j
        alias v\n&&_h d\k
        .set v\n&&_number, \i
    .endm
    do_alias \q, %(\q * 2), %(\q * 2 + 1), \v
    set_sign_aliases \v, \q
.endm
        
.macro reset_v_aliases
    LOCAL loop
    .macro loop i
        set_v_alias \i, \i
    .if \i < 15
        loop %(\i + 1)
    .endif
    .endm
    loop 0
.endm
.macro VSWAP va,vb
    LOCAL n,m
    .set n, \va&_number
    .set m, \vb&_number
    set_v_alias n, m
    set_v_alias m, n
.endm
.macro init_v_aliases
@ q4-q7 (d8-d15) are callee saved, so it's easiest to swap
@ them with q12-q15
    reset_v_aliases
    VSWAP 4, 12
    VSWAP 5, 13
    VSWAP 6, 14
    VSWAP 7, 15
.endm
alias_doubleword_to_quadword 15
