/*
 * Copyright (c) [2022] Texas Instruments Incorporated
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

#include "gsttiovxpyramidbufferpool.h"

#include "gsttiovxallocator.h"
#include "gsttiovxpyramidmeta.h"
#include "gsttiovxutils.h"

/**
 * SECTION:gsttiovxpyramidbufferpool
 * @short_description: GStreamer buffer pool for GstTIOVX Pyramid-based elements
 *
 * This class implements a GStreamer standard buffer pool for GstTIOVX
 * pyramid-based elements.
 */

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_pyramid_buffer_pool_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_pyramid_buffer_pool_debug_category

struct _GstTIOVXPyramidBufferPool
{
  GstTIOVXBufferPool parent;
};

#define gst_tiovx_pyramid_buffer_pool_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXPyramidBufferPool,
    gst_tiovx_pyramid_buffer_pool, GST_TYPE_TIOVX_BUFFER_POOL,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_pyramid_buffer_pool_debug_category,
        "tiovxpyramidbufferpool", 0,
        "debug category for TIOVX pyramid buffer pool class"));

static gboolean gst_tiovx_pyramid_buffer_pool_validate_caps (GstTIOVXBufferPool
    * self, const GstCaps * caps, const vx_reference exemplar);
static void gst_tiovx_pyramid_buffer_pool_add_meta_to_buffer (GstTIOVXBufferPool
    * self, GstBuffer * buffer, vx_reference reference, guint num_channels,
    GstTIOVXMemoryData * ti_memory);
static void gst_tiovx_pyramid_buffer_pool_free_buffer_meta (GstTIOVXBufferPool *
    self, GstBuffer * buffer);

static void
gst_tiovx_pyramid_buffer_pool_class_init (GstTIOVXPyramidBufferPoolClass *
    klass)
{
  GstTIOVXBufferPoolClass *gsttiovxbufferpool_class = NULL;

  gsttiovxbufferpool_class = GST_TIOVX_BUFFER_POOL_CLASS (klass);

  gsttiovxbufferpool_class->validate_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_pyramid_buffer_pool_validate_caps);
  gsttiovxbufferpool_class->add_meta_to_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_pyramid_buffer_pool_add_meta_to_buffer);
  gsttiovxbufferpool_class->free_buffer_meta =
      GST_DEBUG_FUNCPTR (gst_tiovx_pyramid_buffer_pool_free_buffer_meta);
}

static void
gst_tiovx_pyramid_buffer_pool_init (GstTIOVXPyramidBufferPool * self)
{
}


static gboolean
gst_tiovx_pyramid_buffer_pool_validate_caps (GstTIOVXBufferPool * self,
    const GstCaps * caps, const vx_reference exemplar)
{
  return TRUE;
}

void
gst_tiovx_pyramid_buffer_pool_add_meta_to_buffer (GstTIOVXBufferPool * self,
    GstBuffer * buffer, vx_reference exemplar, guint num_channels,
    GstTIOVXMemoryData * ti_memory)
{

}


void
gst_tiovx_pyramid_buffer_pool_free_buffer_meta (GstTIOVXBufferPool * self,
    GstBuffer * buffer)
{

}
