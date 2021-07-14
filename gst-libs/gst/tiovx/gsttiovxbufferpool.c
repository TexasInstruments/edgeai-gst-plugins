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

#include "gsttiovxbufferpool.h"

#include <gst/video/video.h>
#include <TI/tivx.h>

#include "gsttiovxallocator.h"
#include "gsttiovxmeta.h"
#include "gsttiovxutils.h"

/**
 * SECTION:gsttiovxbufferpool
 * @short_description: GStreamer buffer pool for GstTIOVX based elements
 *
 * This class implements a GStreamer standard buffer pool for GstTIOVX
 * based elements.
 */

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_buffer_pool_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_buffer_pool_debug_category

struct _GstTIOVXBufferPool
{
  GstVideoBufferPool base;

  GstTIOVXAllocator *allocator;
  GstVideoInfo caps_info;

  vx_reference exemplar;
};

G_DEFINE_TYPE_WITH_CODE (GstTIOVXBufferPool, gst_tiovx_buffer_pool,
    GST_TYPE_VIDEO_BUFFER_POOL,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_buffer_pool_debug_category,
        "tiovxbufferpool", 0, "debug category for TIOVX buffer pool class"));

/* prototypes */
static GstFlowReturn gst_tiovx_buffer_pool_alloc_buffer (GstBufferPool * pool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params);
static void gst_tiovx_buffer_pool_free_buffer (GstBufferPool * pool,
    GstBuffer * buffer);
static gboolean gst_tiovx_buffer_pool_set_config (GstBufferPool * pool,
    GstStructure * config);
static void gst_tiovx_buffer_pool_finalize (GObject * object);

void gst_buffer_pool_config_set_exemplar(GstStructure * config, const vx_reference exemplar) {
  g_return_if_fail (config != NULL);


  gst_structure_set (config,
      "vx-exemplar", G_TYPE_POINTER, exemplar, NULL);
}


static void gst_buffer_pool_config_get_exemplar(GstStructure * config, vx_reference *exemplar) {
  g_return_if_fail (config != NULL);
  g_return_if_fail (exemplar != NULL);

  gst_structure_get (config,
      "vx-exemplar", G_TYPE_POINTER, exemplar, NULL);

}

static void
gst_tiovx_buffer_pool_class_init (GstTIOVXBufferPoolClass * klass)
{
  GObjectClass *o_class = G_OBJECT_CLASS (klass);
  GstBufferPoolClass *bp_class = GST_BUFFER_POOL_CLASS (klass);

  o_class->finalize = gst_tiovx_buffer_pool_finalize;

  bp_class->alloc_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_buffer_pool_alloc_buffer);
  bp_class->free_buffer = GST_DEBUG_FUNCPTR (gst_tiovx_buffer_pool_free_buffer);
  bp_class->set_config = GST_DEBUG_FUNCPTR (gst_tiovx_buffer_pool_set_config);
}

static void
gst_tiovx_buffer_pool_init (GstTIOVXBufferPool * self)
{
  GST_INFO_OBJECT (self, "New TIOVX buffer pool");
}

static gboolean
gst_tiovx_buffer_pool_validate_caps (GstTIOVXBufferPool * self,
    const GstVideoInfo * video_info, const vx_reference exemplar)
{
  vx_df_image vx_format = VX_DF_IMAGE_VIRT;
  vx_size img_size = 0;
  guint img_width = 0, img_height = 0;
  gboolean ret = FALSE;

  vxQueryImage ((vx_image) exemplar, VX_IMAGE_WIDTH, &img_width,
      sizeof (img_width));
  vxQueryImage ((vx_image) exemplar, VX_IMAGE_HEIGHT, &img_height,
      sizeof (img_height));
  vxQueryImage ((vx_image) exemplar, VX_IMAGE_FORMAT, &vx_format,
      sizeof (vx_format));
  vxQueryImage ((vx_image) exemplar, VX_IMAGE_SIZE, &img_size,
      sizeof (img_size));

  if (img_width != video_info->width) {
    GST_ERROR_OBJECT (self, "Exemplar and caps's width don't match");
    goto out;
  }

  if (img_height != video_info->height) {
    GST_ERROR_OBJECT (self, "Exemplar and caps's height don't match");
    goto out;
  }

  if (vx_format_to_gst_format (vx_format) != video_info->finfo->format) {
    GST_ERROR_OBJECT (self, "Exemplar and caps's format don't match");
    goto out;
  }

  if (img_size != video_info->size) {
    GST_ERROR_OBJECT (self, "Exemplar and caps's sizes don't match");
    goto out;
  }

  ret = TRUE;

out:
  return ret;
}

static gboolean
gst_tiovx_buffer_pool_set_config (GstBufferPool * pool, GstStructure * config)
{
  GstTIOVXBufferPool *self = GST_TIOVX_BUFFER_POOL (pool);
  GstAllocator *allocator = NULL;
  vx_reference exemplar;
  vx_status status;
  GstCaps *caps = NULL;
  guint min_buffers = 0;
  guint max_buffers = 0;
  guint size = 0;

  if (!gst_buffer_pool_config_get_params (config, &caps, &size, &min_buffers,
          &max_buffers)) {
    GST_ERROR_OBJECT (self, "Error getting parameters from buffer pool config");
    goto error;
  }

  if (NULL == caps) {
    GST_ERROR_OBJECT (self, "Requested buffer pool configuration without caps");
    goto error;
  }

  if (!gst_video_info_from_caps (&self->caps_info, caps)) {
    GST_ERROR_OBJECT (self, "Unable to parse caps info");
    goto error;
  }

  gst_buffer_pool_config_get_exemplar(config, &exemplar);
  if (NULL == exemplar) {
    GST_ERROR_OBJECT(self, "Failed to retrieve exemplar from configuration");
    goto error;
  }

  status = vxRetainReference (exemplar);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "VX Reference is not valid");
    goto error;
  }
  self->exemplar = exemplar;

  if (!gst_tiovx_buffer_pool_validate_caps (self, &self->caps_info,
          self->exemplar)) {
    GST_ERROR_OBJECT (self, "Caps and exemplar don't match");
    goto error;
  }

  gst_buffer_pool_config_get_allocator (config, &allocator, NULL);
  if (NULL == allocator) {
    allocator = g_object_new (GST_TIOVX_TYPE_ALLOCATOR, NULL);
    gst_buffer_pool_config_set_allocator (config,
          GST_ALLOCATOR (allocator), NULL);
  } else if (!GST_TIOVX_IS_ALLOCATOR (allocator)) {
    GST_ERROR_OBJECT (self, "Can't use a non-tiovx allocator");
    goto error;
  } else {
    g_object_ref(allocator);
  }

  self->allocator = GST_TIOVX_ALLOCATOR(allocator);

  GST_DEBUG_OBJECT (self,
      "Setting TIOVX pool configuration with caps %" GST_PTR_FORMAT
      " and size %" G_GUINT64_FORMAT, caps, self->caps_info.size);

  return
      GST_BUFFER_POOL_CLASS (gst_tiovx_buffer_pool_parent_class)->set_config
      (pool, config);

error:
  return FALSE;
}

static GstFlowReturn
gst_tiovx_buffer_pool_alloc_buffer (GstBufferPool * pool, GstBuffer ** buffer,
    GstBufferPoolAcquireParams * params)
{
  GstTIOVXBufferPool *self = GST_TIOVX_BUFFER_POOL (pool);
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstBuffer *outbuf = NULL;
  GstMemory *outmem = NULL;
  GstVideoFrameFlags flags = GST_VIDEO_FRAME_FLAG_NONE;
  GstTIOVXMeta *tiovxmeta = NULL;
  GstTIOVXMemoryData *ti_memory = NULL;
  vx_size img_size = 0;

  GST_DEBUG_OBJECT (self, "Allocating TIOVX buffer");

  vxQueryImage ((vx_image) self->exemplar, VX_IMAGE_SIZE, &img_size,
      sizeof (img_size));

  outmem = gst_allocator_alloc (GST_ALLOCATOR (self->allocator), img_size, NULL);
  if (!outmem) {
    GST_ERROR_OBJECT (pool, "Unable to allocate memory");
    goto err_out;
  }

  /* Create output buffer */
  outbuf = gst_buffer_new ();
  gst_buffer_append_memory (outbuf, outmem);

  ti_memory = gst_tiovx_memory_get_data (outmem);
  if (NULL == ti_memory) {
    GST_ERROR_OBJECT (pool, "Unable retrieve TI memory");
    goto free_buffer;
  }

  /* Add meta */
  tiovxmeta = gst_buffer_add_tiovx_meta(outbuf, self->exemplar, ti_memory->mem_ptr.host_ptr);

  gst_buffer_add_video_meta_full (outbuf,
      flags,
      tiovxmeta->image_info.format, tiovxmeta->image_info.width, tiovxmeta->image_info.height, tiovxmeta->image_info.num_planes, tiovxmeta->image_info.plane_offset, tiovxmeta->image_info.plane_strides);

  *buffer = outbuf;
  ret = GST_FLOW_OK;

  goto out;

free_buffer:
  gst_buffer_unref (outbuf);

err_out:
  ret = GST_FLOW_ERROR;

out:
  return ret;
}

static void
gst_tiovx_buffer_pool_free_buffer (GstBufferPool * pool, GstBuffer * buffer)
{
  GstTIOVXMeta *tiovxmeta = NULL;

  tiovxmeta =
      (GstTIOVXMeta *) gst_buffer_get_meta (buffer, GST_TIOVX_META_API_TYPE);
  if (NULL != tiovxmeta) {
    if (NULL != tiovxmeta->array) {
      vxReleaseObjectArray (&tiovxmeta->array);
    }
  }

  GST_BUFFER_POOL_CLASS (gst_tiovx_buffer_pool_parent_class)->free_buffer (pool,
      buffer);
}

static void
gst_tiovx_buffer_pool_finalize (GObject * object)
{
  GstTIOVXBufferPool *self = GST_TIOVX_BUFFER_POOL (object);

  GST_DEBUG_OBJECT (self, "Finalizing TIOVX buffer pool");

  g_clear_object (&self->allocator);

  vxReleaseReference (&self->exemplar);
  self->exemplar = NULL;

  G_OBJECT_CLASS (gst_tiovx_buffer_pool_parent_class)->finalize (object);
}
