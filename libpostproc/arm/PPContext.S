/*********************************************************
* Definition of the PPContext and PPMode structs in assembly
* Copyright (C) 2015 Tucker DiNapoli (T.Dinapoli at gmail.com)
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
*******************************************************************/

@ this should only be included in PPUtil.S
struc PPMode
    field lum_mode, 4
    field chrom_mode, 4
    field error, 4
    field min_allowed_y, 4
    field max_allowed_y, 4
    field max_clipped_threshold, 4
    field max_tmp_noise, 12
    field base_dc_diff, 4
    field flatness_threshold, 4
    field forced_quant, 4
endstruc PPMode

struc PPContext
    field av_class, 4
    field temp_blocks, 4
    field y_historgam, 4
    field packed_yoffset, 8, 8
    field packed_yscale, 8;
    field temp_blurred, 12
    field temp_blurred_past, 12
    field temp_dst, 4
    field temp_src, 4
    field deint_temp, 4
    field pQPb, 8, 8
    field pQPb2, 8, 8
    field pQPb_block, 32, 32
    field pQPb2_block, 32, 32
    field mmx_dc_offset, 512, 32
    field mmx_dc_threshold, 512, 32
    field std_QP_table, 4
    field non_BQP_table, 4
    field forced_QP_table, 4
    field QP, 4
    field nonBQP, 4
    field QP_block, 4
    field nonBQP_block, 4
    field frame_num, 4
    field cpu_caps, 4
    field qp_stride, 4
    field stride, 4
    field h_chroma_subsample, 4
    field v_chroma_subsample, 4
    field ppMode, PPMode_size
endstruc PPContext
