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

#include <math.h>

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
  gboolean ret = FALSE;
  gint caps_levels = 0;
  vx_size query_levels = 0;
  gdouble caps_scale = 0;
  vx_float32 query_scale = 0;
  gint caps_width = 0, caps_height = 0;
  gint query_width = 0, query_height = 0;
  vx_df_image query_format = VX_DF_IMAGE_VIRT;
  GstVideoFormat caps_format = GST_VIDEO_FORMAT_UNKNOWN;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (caps, FALSE);
  g_return_val_if_fail (exemplar, FALSE);

  GST_LOG_OBJECT (self, "Caps to validate: %" GST_PTR_FORMAT, caps);

  if (!gst_tioxv_get_pyramid_caps_info ((GObject *) self, GST_CAT_DEFAULT,
          caps, &caps_levels, &caps_scale, &caps_width, &caps_height,
          &caps_format)) {
    GST_ERROR_OBJECT (self,
        "Couldn't extract pyramid info from caps: %" GST_PTR_FORMAT, caps);
    goto out;
  }

  /* Retrieve pyramid info from exemplar */
  vxQueryPyramid ((vx_pyramid) exemplar, VX_PYRAMID_LEVELS,
      &query_levels, sizeof (query_levels));
  vxQueryPyramid ((vx_pyramid) exemplar, VX_PYRAMID_SCALE,
      &query_scale, sizeof (query_scale));
  vxQueryPyramid ((vx_pyramid) exemplar, VX_PYRAMID_WIDTH,
      &query_width, sizeof (query_width));
  vxQueryPyramid ((vx_pyramid) exemplar, VX_PYRAMID_HEIGHT,
      &query_height, sizeof (query_height));
  vxQueryPyramid ((vx_pyramid) exemplar, VX_PYRAMID_FORMAT,
      &query_format, sizeof (query_format));

  if (caps_levels != query_levels) {
    GST_ERROR_OBJECT (self, "Caps levels %d different to query levels %ld",
        caps_levels, query_levels);
    goto out;
  }
  if (fabs (caps_scale - query_scale) >= FLT_EPSILON) {
    GST_ERROR_OBJECT (self, "Caps scale %f different to query scale %f",
        caps_scale, query_scale);
    goto out;
  }
  if (caps_width != query_width) {
    GST_ERROR_OBJECT (self, "Caps width %d different to query width %d",
        caps_width, query_width);
    goto out;
  }
  if (caps_height != query_height) {
    GST_ERROR_OBJECT (self, "Caps height %d different to query height %d",
        caps_height, query_height);
    goto out;
  }
  if (caps_format != vx_format_to_gst_format (query_format)) {
    GST_ERROR_OBJECT (self, "Caps format different to query format");
    goto out;
  }
  ret = TRUE;

out:
  return ret;
}

void
gst_tiovx_pyramid_buffer_pool_add_meta_to_buffer (GstTIOVXBufferPool * self,
    GstBuffer * buffer, vx_reference exemplar, guint num_channels,
    GstTIOVXMemoryData * ti_memory)
{
  g_return_if_fail (self);
  g_return_if_fail (buffer);
  g_return_if_fail (exemplar);
  g_return_if_fail (num_channels >= MIN_NUM_CHANNELS);
  g_return_if_fail (num_channels <= MAX_NUM_CHANNELS);
  g_return_if_fail (ti_memory);

  gst_buffer_add_tiovx_pyramid_meta (buffer, exemplar, num_channels,
      ti_memory->mem_ptr.host_ptr);
}


void
gst_tiovx_pyramid_buffer_pool_free_buffer_meta (GstTIOVXBufferPool * self,
    GstBuffer * buffer)
{
  GstTIOVXPyramidMeta *tiovxmeta = NULL;
  vx_reference ref = NULL;

  g_return_if_fail (self);
  g_return_if_fail (buffer);

  tiovxmeta =
      (GstTIOVXPyramidMeta *) gst_buffer_get_meta (buffer,
      GST_TYPE_TIOVX_PYRAMID_META_API);
  if (NULL != tiovxmeta) {
    if (NULL != tiovxmeta->array) {
      vx_size num_channels = 0;
      gint i = 0;

      vxQueryObjectArray (tiovxmeta->array, VX_OBJECT_ARRAY_NUMITEMS,
          &num_channels, sizeof (num_channels));

      for (i = 0; i < num_channels; i++) {
        ref = vxGetObjectArrayItem (tiovxmeta->array, i);
        gst_tiovx_empty_exemplar (ref);
        vxReleaseReference (&ref);
      }

      vxReleaseObjectArray (&tiovxmeta->array);
    }
  }
}
