/*
 * Copyright (c) [2021] Texas Instruments Incorporated
 *
 * All rights reserved not granted herein.
 *
 * Limited License.
 *
 * Texas Instruments Incorporated grants a world-wide, royalty-free,
 * non-exclusive license under copyrights and patents it now or hereafter
 * owns or controls to make, have made, use, import, offer to sell and sell
 * ("Utilize") this software subject to the terms herein.  With respect to
 * the foregoing patent license, such license is granted  solely to the extent
 * that any such patent is necessary to Utilize the software alone.
 * The patent license shall not apply to any combinations which include
 * this software, other than combinations with devices manufactured by or
 * for TI (“TI Devices”).  No hardware patent is licensed hereunder.
 *
 * Redistributions must preserve existing copyright notices and reproduce
 * this license (including the above copyright notice and the disclaimer
 * and (if applicable) source code license limitations below) in the
 * documentation and/or other materials provided with the distribution
 *
 * Redistribution and use in binary form, without modification, are permitted
 * provided that the following conditions are met:
 *
 * *	No reverse engineering, decompilation, or disassembly of this software
 *      is permitted with respect to any software provided in binary form.
 *
 * *	Any redistribution and use are licensed by TI for use only with TI
 * Devices.
 *
 * *	Nothing shall obligate TI to provide you with source code for the
 *      software licensed and provided to you in object code.
 *
 * If software source code is provided to you, modification and redistribution
 * of the source code are permitted provided that the following conditions are
 * met:
 *
 * *	Any redistribution and use of the source code, including any resulting
 *      derivative works, are licensed by TI for use only with TI Devices.
 *
 * *	Any redistribution and use of any object code compiled from the source
 *      code and any resulting derivative works, are licensed by TI for use
 *      only with TI Devices.
 *
 * Neither the name of Texas Instruments Incorporated nor the names of its
 * suppliers may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * DISCLAIMER.
 *
 * THIS SOFTWARE IS PROVIDED BY TI AND TI’S LICENSORS "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TI AND TI’S LICENSORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __GST_TIOVX_UTILS_H__
#define __GST_TIOVX_UTILS_H__

#include "gsttiovxbufferpool.h"

#include <gst/video/video.h>
#include <VX/vx.h>
#include <VX/vx_types.h>

/**
 * vx_format_to_gst_format:
 * @format: format to convert
 *
 * Converts a vx_df_image to a #GstVideoFormat
 *
 * Returns: Converted format
 *
 */
GstVideoFormat vx_format_to_gst_format (const vx_df_image format);

/**
 * gst_format_to_vx_format:
 * @gst_format: format to convert
 *
 * Converts a #GstVideoFormat to a vx_df_image
 *
 * Returns: Converted format
 *
 */
vx_df_image gst_format_to_vx_format (const GstVideoFormat gst_format);

/**
 * gst_tiovx_transfer_handle:
 * @self: Object using this function
 * @src: Reference where the handles will be transfer from.
 * @dest: Reference where the handles will be transfer to.
 *
 * Transfers handles between to vx_references
 *
 * Returns: VX_SUCCESS is the data was successfully transferd
 *
 */
vx_status
gst_tiovx_transfer_handle (GstObject * self, vx_reference src,
    vx_reference dest);

/**
 * gst_tiovx_add_new_pool:
 * @category: Category to use for debug messages
 * @query: Query where the pool will be added
 * @num_buffers: Number of buffers for the pool
 * @exemplar: Exemplar to be used as a reference for the pool
 * @info: Video information to be used as a reference for the pool
 *
 * Returns: True if the pool was successfully added
 *
 */
gboolean
gst_tiovx_add_new_pool (GstDebugCategory * category, GstQuery * query,
    guint num_buffers, vx_reference * exemplar, GstVideoInfo * info);

/**
 * gst_tiovx_buffer_copy:
 * @self: Object using this function
 * @pool: Pool from which to extract output buffer to copy into
 * @in_buffer: Input buffer that will be copied
 *
 * This function doesn't take ownership of in_buffer
 *
 * Returns: Copy of GstBuffer from input buffer
 *
 */
GstBuffer *
gst_tiovx_buffer_copy (GstObject * self, GstTIOVXBufferPool * pool,
    GstBuffer * in_buffer);

#endif /* __GST_TIOVX_UTILS_H__ */
