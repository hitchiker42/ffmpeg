;*
;* Definition of the PPContext and PPMode structs in assembly
;* Copyright (C) 2015 Tucker DiNapoli (T.Dinapoli at gmail.com)
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
%if ARCH_X86_64
%define pointer resq
%else
%define pointer resd
%endif
struc PPMode
    .lum_mode: resd 1
    .chrom_mode: resd 1
    .error: resd 1
    .min_allowed_y: resd 1
    .max_allowed_y: resd 1
    .max_clipped_threshold: resd 1
    .max_tmp_noise: resd 3
    .base_dc_diff: resd 1
    .flatness_threshold: resd 1
    .forced_quant: resd 1
endstruc

struc PPContext
    .av_class pointer 1
    .temp_blocks pointer 1
    .y_historgam pointer 1
    alignb 8
    .packed_yoffset resq 1
    .packed_yscale resq 1; 8 byte aligned by default
    .temp_blurred pointer 3
    .temp_blurred_past pointer 3
    .temp_dst pointer 1
    .temp_src pointer 1
    .deint_temp pointer 1
    alignb 8
    .pQPb resq 1
    .pQPb2 resq 1
    alignb 32
    .pQPb_block resq 4
    alignb 32
    .pQPb2_block resq 4
;; These next fields & next alignment may need to be changed for 128/256 bit registers
    alignb 8
    .mmx_dc_offset resq 64
    .mmx_dc_threshold resq 64
    .std_QP_table pointer 1
    .non_BQP_table pointer 1
    .forced_QP_table pointer 1
    .QP resd 1
    .nonBQP resd 1
    .QP_block resb 4
    .nonBQP_block resb 4
    .frame_num resd 1
    .cpu_caps resd 1
    .qp_stride resd 1
    .stride resd 1
    .h_chroma_subsample resd 1
    .v_chroma_subsample resd 1
    .ppMode resb PPMode_size
endstruc
