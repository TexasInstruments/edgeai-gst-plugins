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


#define TIOVX_ARRAY_LENGHT 1
#define APP_MODULES_MAX_NUM_ADDR 4

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

  vx_reference reference;
};

G_DEFINE_TYPE_WITH_CODE (GstTIOVXBufferPool, gst_tiovx_buffer_pool,
    GST_TYPE_VIDEO_BUFFER_POOL,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_buffer_pool_debug_category,
        "tiovxbufferpool", 0, "debug category for TIOVX buffer pool class"));

/* prototypes */
static GstFlowReturn gst_tiovx_buffer_pool_alloc_buffer (GstBufferPool * pool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params);
static gboolean gst_tiovx_buffer_pool_set_config (GstBufferPool * pool,
    GstStructure * config);
static void gst_tiovx_buffer_pool_finalize (GObject * object);

GstTIOVXBufferPool *
gst_tiovx_buffer_pool_new (const vx_reference reference)
{
  GstTIOVXBufferPool *pool = g_object_new (GST_TIOVX_TYPE_BUFFER_POOL, NULL);

  pool->reference = reference;

  return pool;
}

static void
gst_tiovx_buffer_pool_class_init (GstTIOVXBufferPoolClass * klass)
{
  GObjectClass *o_class = G_OBJECT_CLASS (klass);
  GstBufferPoolClass *bp_class = GST_BUFFER_POOL_CLASS (klass);

  o_class->finalize = gst_tiovx_buffer_pool_finalize;

  bp_class->alloc_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_buffer_pool_alloc_buffer);
  bp_class->set_config = GST_DEBUG_FUNCPTR (gst_tiovx_buffer_pool_set_config);
}

static void
gst_tiovx_buffer_pool_init (GstTIOVXBufferPool * self)
{
  GST_INFO_OBJECT (self, "New TIOVX buffer pool");

  self->allocator = g_object_new (GST_TIOVX_TYPE_ALLOCATOR, NULL);
}

static gboolean
gst_tiovx_buffer_pool_set_config (GstBufferPool * pool, GstStructure * config)
{
  GstTIOVXBufferPool *self = GST_TIOVX_BUFFER_POOL (pool);
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

  GST_DEBUG_OBJECT (self,
      "Setting TIOVX pool configuration with caps %" GST_PTR_FORMAT
      " and size %lu", caps, self->caps_info.size);

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
  GstTIOVXMeta *tiovxmeta = NULL;
  tivx_shared_mem_ptr_t *mem_ptr = NULL;
  void **addr = NULL;
  void *plane_addr[APP_MODULES_MAX_NUM_ADDR] = { NULL };
  vx_image ref;
  vx_size img_size = 0;
  vx_status status;
  vx_uint32 plane_sizes[APP_MODULES_MAX_NUM_ADDR];
  guint num_planes = 0;
  guint plane_idx = 0;
  gint prev_size = 0;

  GST_DEBUG_OBJECT (self, "Allocating TIOVX buffer");

  status = vxQueryImage ((vx_image) self->reference, VX_IMAGE_SIZE, &img_size,
      sizeof (img_size));
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Unable to query image size");
    goto out;
  }

  outmem =
      gst_allocator_alloc (GST_ALLOCATOR (self->allocator), img_size, NULL);
  if (!outmem) {
    GST_ERROR_OBJECT (pool, "Unable to allocate TIOVX buffer");
    goto out;
  }

  mem_ptr =
      gst_mini_object_get_qdata (GST_MINI_OBJECT_CAST (outmem),
      TIOVX_MEM_PTR_QUARK);
  if (NULL == mem_ptr) {
    gst_allocator_free (GST_ALLOCATOR (self->allocator), outmem);
    ret = GST_FLOW_ERROR;
    goto out;
  }

  /* Create output buffer */
  outbuf = gst_buffer_new ();
  gst_buffer_append_memory (outbuf, outmem);

  /* Get plane and size information */
  tivxReferenceExportHandle ((vx_reference) self->reference,
      plane_addr, plane_sizes, APP_MODULES_MAX_NUM_ADDR, &num_planes);

  addr = g_malloc (sizeof (void *) * num_planes);
  for (plane_idx = 0; plane_idx < num_planes; plane_idx++) {
    addr[plane_idx] = (void *) (mem_ptr->host_ptr + prev_size);

    prev_size = plane_sizes[plane_idx];
  }

  /* Add meta */
  tiovxmeta =
      (GstTIOVXMeta *) gst_buffer_add_meta (outbuf,
      gst_tiovx_meta_get_info (), NULL);

  /* Create vx object array */
  tiovxmeta->array =
      vxCreateObjectArray (vxGetContext (self->reference), self->reference,
      TIOVX_ARRAY_LENGHT);

  /* Import memory into the meta's vx reference */
  ref = (vx_image) vxGetObjectArrayItem (tiovxmeta->array, 0);
  status =
      tivxReferenceImportHandle ((vx_reference) ref, (const void **) addr,
      plane_sizes, num_planes);
  g_free (addr);
  if (status != VX_SUCCESS) {
    ret = GST_FLOW_ERROR;
    GST_ERROR_OBJECT (pool,
        "Unable to import tivx_shared_mem_ptr to a vx_image: %d", status);
    gst_buffer_unref (outbuf);
    goto out;
  }

  *buffer = outbuf;
  ret = GST_FLOW_OK;

out:
  return ret;
}

static void
gst_tiovx_buffer_pool_finalize (GObject * object)
{
  GstTIOVXBufferPool *self = GST_TIOVX_BUFFER_POOL (object);

  GST_DEBUG_OBJECT (self, "Finalizing TIOVX buffer pool");

  g_clear_object (&self->allocator);

  vxReleaseReference (&self->reference);

  G_OBJECT_CLASS (gst_tiovx_buffer_pool_parent_class)->finalize (object);
}
