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
 * *	Any redistribution and use are licensed by TI for use only with TI Devices.
 * 
 * *	Nothing shall obligate TI to provide you with source code for the
 *      software licensed and provided to you in object code.
 * 
 * If software source code is provided to you, modification and redistribution
 * of the source code are permitted provided that the following conditions are met:
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

#ifndef __GST_TIOVX_RAW_IMAGE_META__
#define __GST_TIOVX_RAW_IMAGE_META__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <TI/tivx.h>

#define MODULE_MAX_NUM_EXPOSURES 3

G_BEGIN_DECLS 

#define GST_TYPE_TIOVX_RAW_IMAGE_META_API (gst_tiovx_raw_image_meta_api_get_type())
#define GST_TIOVX_RAW_IMAGE_META_INFO  (gst_tiovx_raw_image_meta_get_info())

typedef struct _GstTIOVXRawImageInfo GstTIOVXRawImageInfo;
struct _GstTIOVXRawImageInfo {
  guint num_exposures;
  gsize exposure_offset[MODULE_MAX_NUM_EXPOSURES];
  gint exposure_stride_x[MODULE_MAX_NUM_EXPOSURES];
  gint exposure_stride_y[MODULE_MAX_NUM_EXPOSURES];
  guint exposure_sizes[MODULE_MAX_NUM_EXPOSURES];
  guint exposure_steps_x[MODULE_MAX_NUM_EXPOSURES];
  guint exposure_steps_y[MODULE_MAX_NUM_EXPOSURES];
  guint exposure_widths[MODULE_MAX_NUM_EXPOSURES];
  guint exposure_heights[MODULE_MAX_NUM_EXPOSURES];
};


/**
 * GstTIOVXRawImageMeta:
 * @meta: parent #GstMeta
 * @array: VX Object Array holding the number of images in the batch
 * @image_info: Information for the held images
 *
 * TIOVX Meta hold OpenVX related information
 */
typedef struct _GstTIOVXRawImageMeta GstTIOVXRawImageMeta;
struct _GstTIOVXRawImageMeta {
  GstMeta meta;

  vx_object_array array;
  GstTIOVXRawImageInfo image_info;
};

/**
 * gst_tiovx_meta_api_get_type:
 * 
 * Gets the type for the TIOVX Raw Image Meta
 * 
 * Returns: type of TIOVX Raw Image Meta
 * 
 */
GType gst_tiovx_raw_image_meta_api_get_type (void);

/**
 * gst_tiovx_meta_get_info:
 * 
 * Gets the TIOXV Raw Image Meta Meta's GstMetaInfo
 * 
 * Returns: MetaInfo for TIOVX Meta
 * 
 */
const GstMetaInfo *gst_tiovx_raw_image_meta_get_info (void);

/**
 * gst_buffer_add_tiovx_raw_image meta:
 * @buffer: Buffer where the meta will be added
 * @exemplar: Exemplar to be added to the meta
 * @mem_start: Pointer where the memory for the image starts 
 * 
 * Adds a raw image meta to the buffer and initializes the related structures
 * 
 * Returns: Raw image meta that was added to the buffer
 * 
 */
GstTIOVXRawImageMeta* gst_buffer_add_tiovx_raw_image_meta(GstBuffer* buffer, const vx_reference exemplar, guint64 mem_start);

G_END_DECLS


#endif /* __GST_TIOVX_RAW_IMAGE_META__ */
