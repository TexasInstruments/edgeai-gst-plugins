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


#include "gsttiovxbufferpoolutils.h"

#include "gsttiovx.h"
#include "gsttiovximagebufferpool.h"
#include "gsttiovxpyramidbufferpool.h"
#include "gsttiovxrawimagebufferpool.h"
#include "gsttiovxtensorbufferpool.h"
#include "gsttiovxutils.h"

/* Creates a new pool based on exemplar */
GstBufferPool *
gst_tiovx_create_new_pool (GstDebugCategory * category, vx_reference exemplar)
{
  GstBufferPool *pool = NULL;
  vx_enum type = VX_TYPE_INVALID;

  g_return_val_if_fail (category, NULL);
  g_return_val_if_fail (exemplar, NULL);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus (exemplar), NULL);

  GST_CAT_INFO (category, "Creating new pool");

  /* Getting reference type */
  type = gst_tiovx_get_exemplar_type (exemplar);

  if (VX_TYPE_IMAGE == type) {
    GST_CAT_INFO (category, "Creating Image buffer pool");
    pool = g_object_new (GST_TYPE_TIOVX_IMAGE_BUFFER_POOL, NULL);
  } else if (VX_TYPE_TENSOR == type) {
    GST_CAT_INFO (category, "Creating Tensor buffer pool");
    pool = g_object_new (GST_TYPE_TIOVX_TENSOR_BUFFER_POOL, NULL);
  } else if (TIVX_TYPE_RAW_IMAGE == type) {
    GST_CAT_INFO (category, "Creating Raw Image buffer pool");
    pool = g_object_new (GST_TYPE_TIOVX_RAW_IMAGE_BUFFER_POOL, NULL);
  } else if (VX_TYPE_PYRAMID == type) {
    GST_CAT_INFO (category, "Creating Pyramid buffer pool");
    pool = g_object_new (GST_TYPE_TIOVX_PYRAMID_BUFFER_POOL, NULL);
  } else {
    GST_CAT_ERROR (category,
        "Type %d not supported, buffer pool was not created", type);
  }

  return pool;
}

/* Adds a pool to the query */
gboolean
gst_tiovx_add_new_pool (GstDebugCategory * category, GstQuery * query,
    guint num_buffers, vx_reference exemplar, gsize size, gint num_channels,
    GstBufferPool ** buffer_pool)
{
  GstCaps *caps = NULL;
  GstBufferPool *pool = NULL;

  g_return_val_if_fail (category, FALSE);
  g_return_val_if_fail (query, FALSE);
  g_return_val_if_fail (exemplar, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus (exemplar), FALSE);
  g_return_val_if_fail (size > 0, FALSE);
  g_return_val_if_fail (num_channels >= 0, FALSE);

  GST_CAT_DEBUG (category, "Adding new pool");

  pool = gst_tiovx_create_new_pool (category, exemplar);

  if (NULL == pool) {
    GST_CAT_ERROR (category, "Create TIOVX pool failed");
    return FALSE;
  }

  gst_query_parse_allocation (query, &caps, NULL);

  if (!gst_tiovx_configure_pool (category, pool, exemplar, caps, size,
          num_buffers, num_channels)) {
    GST_CAT_ERROR (category, "Unable to configure pool");
    gst_object_unref (pool);
    return FALSE;
  }

  GST_CAT_INFO (category, "Adding new TIOVX pool with %d buffers of %ld size",
      num_buffers, size);

  gst_query_add_allocation_pool (query, pool, size, num_buffers, num_buffers);

  if (buffer_pool) {
    *buffer_pool = pool;
  } else {
    gst_object_unref (pool);
  }

  return TRUE;
}

/* Sets configuration on the buffer pool */
gboolean
gst_tiovx_configure_pool (GstDebugCategory * category, GstBufferPool * pool,
    vx_reference exemplar, GstCaps * caps, gsize size, guint num_buffers,
    gint num_channels)
{
  GstStructure *config = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (category, FALSE);
  g_return_val_if_fail (pool, FALSE);
  g_return_val_if_fail (exemplar, FALSE);
  g_return_val_if_fail (caps, FALSE);
  g_return_val_if_fail (size > 0, FALSE);
  g_return_val_if_fail (num_buffers > 0, FALSE);
  g_return_val_if_fail (num_channels >= 0, FALSE);

  config = gst_buffer_pool_get_config (pool);

  gst_tiovx_buffer_pool_config_set_exemplar (config, exemplar);

  gst_tiovx_buffer_pool_config_set_num_channels (config, num_channels);

  gst_buffer_pool_config_set_params (config, caps, size, num_buffers,
      num_buffers);

  if (!gst_buffer_pool_set_active (GST_BUFFER_POOL (pool), FALSE)) {
    GST_CAT_ERROR (category,
        "Unable to set pool to inactive for configuration");
    gst_object_unref (pool);
    goto exit;
  }

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_CAT_ERROR (category, "Unable to set pool configuration");
    gst_object_unref (pool);
    goto exit;
  }

  ret = TRUE;

exit:
  return ret;
}

/* Gets an exemplar from a TIOVX bufferpool configuration */
void
gst_tiovx_buffer_pool_config_get_exemplar (GstStructure * config,
    vx_reference * exemplar)
{
  g_return_if_fail (config != NULL);
  g_return_if_fail (exemplar != NULL);

  gst_structure_get (config, "vx-exemplar", G_TYPE_INT64, exemplar, NULL);
}

/* Sets an exemplar to a TIOVX bufferpool configuration */
void
gst_tiovx_buffer_pool_config_set_exemplar (GstStructure * config,
    const vx_reference exemplar)
{
  g_return_if_fail (config != NULL);

  gst_structure_set (config, "vx-exemplar", G_TYPE_INT64, exemplar, NULL);
}

/* Gets the number of channels from a TIOVX bufferpool configuration */
void
gst_tiovx_buffer_pool_config_set_num_channels (GstStructure * config,
    const guint num_channels)
{
  g_return_if_fail (config != NULL);

  gst_structure_set (config, "num-channels", G_TYPE_UINT, num_channels, NULL);
}

/* Sets the number of channels to a TIOVX bufferpool configuration */
void
gst_tiovx_buffer_pool_config_get_num_channels (GstStructure * config,
    guint * num_channels)
{
  g_return_if_fail (config != NULL);
  g_return_if_fail (num_channels != NULL);

  gst_structure_get_uint (config, "num-channels", num_channels);
}
