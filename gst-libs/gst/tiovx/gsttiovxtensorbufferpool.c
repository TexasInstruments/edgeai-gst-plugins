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

#include "gsttiovxtensorbufferpool.h"

#include <TI/tivx.h>

#include "gsttiovxallocator.h"
#include "gsttiovxbufferpoolutils.h"
#include "gsttiovxtensormeta.h"
#include "gsttiovxutils.h"

/**
 * SECTION:gsttiovxtensorbufferpool
 * @short_description: GStreamer buffer pool for GstTIOVX Tensor-based elements
 *
 * This class implements a GStreamer standard buffer pool for GstTIOVX
 * tensor-based elements.
 */

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_tensor_buffer_pool_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_tensor_buffer_pool_debug_category

struct _GstTIOVXTensorBufferPool
{
  GstBufferPool base;

  GstTIOVXAllocator *allocator;

  vx_reference exemplar;
};

G_DEFINE_TYPE_WITH_CODE (GstTIOVXTensorBufferPool, gst_tiovx_tensor_buffer_pool,
    GST_TYPE_BUFFER_POOL,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_tensor_buffer_pool_debug_category,
        "tiovxtensorbufferpool", 0,
        "debug category for TIOVX tensor buffer pool class"));

static gboolean gst_tiovx_tensor_buffer_pool_set_config (GstBufferPool * pool,
    GstStructure * config);
static GstFlowReturn gst_tiovx_tensor_buffer_pool_alloc_buffer (GstBufferPool *
    pool, GstBuffer ** buffer, GstBufferPoolAcquireParams * params);
static void gst_tiovx_tensor_buffer_pool_free_buffer (GstBufferPool * pool,
    GstBuffer * buffer);
static void gst_tiovx_tensor_buffer_pool_finalize (GObject * object);

static gboolean
gst_tiovx_tensor_buffer_pool_validate_caps (GstTIOVXTensorBufferPool * self,
    GstCaps * caps, const vx_reference exemplar)
{
  gboolean ret = FALSE;
  gint caps_num_dims = 0;
  vx_size query_num_dims = 0;
  gint caps_data_type = 0;
  vx_enum query_data_type = 0;
  const GstStructure *tensor_s = NULL;
  const gchar *s_name = NULL;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (caps, FALSE);
  g_return_val_if_fail (exemplar, FALSE);

  tensor_s = gst_caps_get_structure (caps, 0);
  if (!tensor_s) {
    goto out;
  }

  GST_LOG_OBJECT (self, "Caps to validate: %" GST_PTR_FORMAT, caps);

  s_name = gst_structure_get_name (tensor_s);
  if (0 != g_strcmp0 (s_name, "application/x-tensor-tiovx")) {
    GST_ERROR_OBJECT (self, "No tensor caps");
    goto out;
  }

  if (!gst_structure_get_int (tensor_s, "num-dims", &caps_num_dims)) {
    GST_ERROR_OBJECT (self, "num-dims not found in tensor caps");
    goto out;
  }
  if (!gst_structure_get_int (tensor_s, "data-type", &caps_data_type)) {
    GST_ERROR_OBJECT (self, "data-type not found in tensor caps");
    goto out;
  }

  vxQueryTensor ((vx_tensor) exemplar, VX_TENSOR_NUMBER_OF_DIMS,
      &query_num_dims, sizeof (vx_size));
  vxQueryTensor ((vx_tensor) exemplar, VX_TENSOR_DATA_TYPE, &query_data_type,
      sizeof (vx_enum));

  if (caps_num_dims != query_num_dims) {
    GST_ERROR_OBJECT (self, "Caps num-dims %d different to query num dims %ld",
        caps_num_dims, query_num_dims);
    goto out;
  }
  if (caps_data_type != query_data_type) {
    GST_ERROR_OBJECT (self, "Caps data-type %d different to query data-type %d",
        caps_data_type, query_data_type);
    goto out;
  }

  ret = TRUE;

out:
  return ret;
}

static void
gst_tiovx_tensor_buffer_pool_class_init (GstTIOVXTensorBufferPoolClass * klass)
{
  GObjectClass *o_class = G_OBJECT_CLASS (klass);
  GstBufferPoolClass *bp_class = GST_BUFFER_POOL_CLASS (klass);

  o_class->finalize = gst_tiovx_tensor_buffer_pool_finalize;

  bp_class->alloc_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_tensor_buffer_pool_alloc_buffer);
  bp_class->free_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_tensor_buffer_pool_free_buffer);
  bp_class->set_config =
      GST_DEBUG_FUNCPTR (gst_tiovx_tensor_buffer_pool_set_config);
}

static void
gst_tiovx_tensor_buffer_pool_init (GstTIOVXTensorBufferPool * self)
{
  GST_INFO_OBJECT (self, "New TIOVX tensor buffer pool");

  self->allocator = NULL;
  self->exemplar = NULL;
}

/* BufferPool Functions */

static gboolean
gst_tiovx_tensor_buffer_pool_set_config (GstBufferPool * pool,
    GstStructure * config)
{
  GstTIOVXTensorBufferPool *self = GST_TIOVX_TENSOR_BUFFER_POOL (pool);
  GstAllocator *allocator = NULL;
  vx_reference exemplar = NULL;
  vx_status status = VX_SUCCESS;
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

  gst_tiovx_buffer_pool_config_get_exemplar (config, &exemplar);

  if (NULL == exemplar) {
    GST_ERROR_OBJECT (self, "Failed to retrieve exemplar from configuration");
    goto error;
  }

  status = vxRetainReference (exemplar);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "VX Reference is not valid");
    goto error;
  }

  if (NULL != self->exemplar) {
    gst_tiovx_empty_exemplar (self->exemplar);
    vxReleaseReference (&self->exemplar);
    self->exemplar = NULL;
  }

  self->exemplar = exemplar;

  /* Validate caps with exemplar */
  if (!gst_tiovx_tensor_buffer_pool_validate_caps (self, caps, self->exemplar)) {
    GST_ERROR_OBJECT (self, "Caps and exemplar don't match");
    goto error;
  }

  gst_buffer_pool_config_get_allocator (config, &allocator, NULL);
  if (NULL == allocator) {
    allocator = g_object_new (GST_TYPE_TIOVX_ALLOCATOR, NULL);
    gst_buffer_pool_config_set_allocator (config,
        GST_ALLOCATOR (allocator), NULL);
  } else if (!GST_TIOVX_IS_ALLOCATOR (allocator)) {
    GST_ERROR_OBJECT (self, "Can't use a non-tiovx allocator");
    goto error;
  } else {
    g_object_ref (allocator);
  }

  self->allocator = GST_TIOVX_ALLOCATOR (allocator);

  GST_DEBUG_OBJECT (self,
      "Setting TIOVX tensor pool configuration with caps %" GST_PTR_FORMAT,
      caps);

  return
      GST_BUFFER_POOL_CLASS
      (gst_tiovx_tensor_buffer_pool_parent_class)->set_config (pool, config);

error:
  return FALSE;
}

static GstFlowReturn
gst_tiovx_tensor_buffer_pool_alloc_buffer (GstBufferPool * pool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params)
{
  GstTIOVXTensorBufferPool *self = GST_TIOVX_TENSOR_BUFFER_POOL (pool);
  GstFlowReturn ret = GST_FLOW_ERROR;
  vx_size tensor_size = 0;
  vx_size num_dims = 0;
  vx_size dims[MODULE_MAX_NUM_DIMS];
  vx_enum data_type = 0;
  GstMemory *outmem = NULL;
  GstBuffer *outbuf = NULL;
  GstTIOVXMemoryData *ti_memory = NULL;
  GstTIOVXTensorMeta *tiovxmeta = NULL;
  guint i = 0;

  if (NULL == self->exemplar) {
    GST_ERROR_OBJECT (self,
        "Failed to alloc tensor buffer, invalid exemplar reference");
    return ret;
  }

  GST_DEBUG_OBJECT (self, "Allocating TIOVX tensor buffer");

  /* Calculate tensor size */
  vxQueryTensor ((vx_tensor) self->exemplar, VX_TENSOR_NUMBER_OF_DIMS,
      &num_dims, sizeof (num_dims));

  if (0 > num_dims) {
    GST_ERROR_OBJECT (self,
        "Failed to alloc tensor buffer, invalid number of tensor dimensions");
    return ret;
  }

  vxQueryTensor ((vx_tensor) self->exemplar, VX_TENSOR_DIMS, &dims,
      num_dims * sizeof (num_dims));

  GST_INFO_OBJECT (self, "Number of dims in tensor: %zu", num_dims);

  vxQueryTensor ((vx_tensor) self->exemplar, VX_TENSOR_DATA_TYPE, &data_type,
      sizeof (vx_enum));

  tensor_size = gst_tiovx_tensor_get_tensor_bit_depth (data_type);

  GST_INFO_OBJECT (self, "Tensor data type: %u with size: %ld", data_type,
      tensor_size);

  for (i = 0; i < num_dims; i++) {
    GST_INFO_OBJECT (self, "Dimension %u of size %zu", i, dims[i]);
    tensor_size *= dims[i];
  }

  GST_INFO_OBJECT (self, "TIOVX Tensor buffer size to alloc: %lu", tensor_size);

  outmem =
      gst_allocator_alloc (GST_ALLOCATOR (self->allocator), tensor_size, NULL);
  if (!outmem) {
    GST_ERROR_OBJECT (self, "Unable to allocate memory");
    goto err_out;
  }

  /* Create output buffer */
  outbuf = gst_buffer_new ();
  gst_buffer_append_memory (outbuf, outmem);

  ti_memory = gst_tiovx_memory_get_data (outmem);
  if (NULL == ti_memory) {
    GST_ERROR_OBJECT (self, "Unable retrieve TI memory");
    goto free_buffer;
  }

  /* Add tensor meta */
  tiovxmeta =
      gst_buffer_add_tiovx_tensor_meta (outbuf, self->exemplar,
      ti_memory->mem_ptr.host_ptr);

  if (NULL == tiovxmeta) {
    GST_ERROR_OBJECT (self, "Unable to add meta");
    goto free_buffer;
  }

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
gst_tiovx_tensor_buffer_pool_free_buffer (GstBufferPool * pool,
    GstBuffer * buffer)
{
  GstTIOVXTensorMeta *tiovxmeta = NULL;
  vx_reference ref = NULL;

  tiovxmeta =
      (GstTIOVXTensorMeta *) gst_buffer_get_meta (buffer,
      GST_TYPE_TIOVX_TENSOR_META_API);
  if (NULL != tiovxmeta) {
    if (NULL != tiovxmeta->array) {
      /* We currently support a single channel */
      ref = vxGetObjectArrayItem (tiovxmeta->array, 0);
      gst_tiovx_empty_exemplar (ref);
      vxReleaseReference (&ref);

      vxReleaseObjectArray (&tiovxmeta->array);
    }
  }

  GST_BUFFER_POOL_CLASS (gst_tiovx_tensor_buffer_pool_parent_class)->free_buffer
      (pool, buffer);
}

static void
gst_tiovx_tensor_buffer_pool_finalize (GObject * object)
{
  GstTIOVXTensorBufferPool *self = GST_TIOVX_TENSOR_BUFFER_POOL (object);

  GST_DEBUG_OBJECT (self, "Finalizing TIOVX tensor buffer pool");

  g_clear_object (&self->allocator);

  if (NULL != self->exemplar) {
    gst_tiovx_empty_exemplar (self->exemplar);
    vxReleaseReference (&self->exemplar);
    self->exemplar = NULL;
  }

  G_OBJECT_CLASS (gst_tiovx_tensor_buffer_pool_parent_class)->finalize (object);
}
