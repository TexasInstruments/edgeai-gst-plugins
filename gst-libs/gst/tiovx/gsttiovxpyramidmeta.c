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

#include "gsttiovxpyramidmeta.h"

#include <TI/tivx.h>

#include "gsttiovxutils.h"

static gboolean gst_tiovx_pyramid_meta_init (GstMeta * meta,
    gpointer params, GstBuffer * buffer);

GType
gst_tiovx_pyramid_meta_api_get_type (void)
{
  static volatile GType type = 0;
  static const gchar *tags[] =
      /* No Video tag needed */
  { GST_META_TAG_MEMORY_STR, NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstTIOVXPyramidMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_tiovx_pyramid_meta_get_info (void)
{
  static const GstMetaInfo *info = NULL;

  if (g_once_init_enter (&info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_TYPE_TIOVX_PYRAMID_META_API,
        "GstTIOVXPyramidMeta",
        sizeof (GstTIOVXPyramidMeta),
        gst_tiovx_pyramid_meta_init,
        NULL,
        NULL);
    g_once_init_leave (&info, meta);
  }
  return info;
}

static gboolean
gst_tiovx_pyramid_meta_init (GstMeta * meta, gpointer params,
    GstBuffer * buffer)
{
  /* Gst requires this func to be implemented, even if it is empty */
  return TRUE;
}

GstTIOVXPyramidMeta *
gst_buffer_add_tiovx_pyramid_meta (GstBuffer * buffer,
    const vx_reference exemplar, const gint array_length, guint64 mem_start)
{
  GstTIOVXPyramidMeta *tiovx_pyramid_meta = NULL;
  vx_size levels = 0;
  vx_float32 scale = 0;
  guint width = 0;
  guint height = 0;
  vx_df_image vx_format = VX_DF_IMAGE_VIRT;
  void *addr[MODULE_MAX_NUM_PYRAMIDS] = { NULL };
  void *pyramid_addr[MODULE_MAX_NUM_PYRAMIDS] = { NULL };
  vx_uint32 pyramid_size[MODULE_MAX_NUM_PYRAMIDS] = { 0 };
  guint num_entries = 0;
  gint prev_size = 0;
  vx_status status = VX_SUCCESS;
  vx_object_array array = NULL;
  gint i = 0;
  gint j = 0;
  gint channel_size = 0;

  g_return_val_if_fail (buffer, NULL);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) exemplar),
      NULL);
  g_return_val_if_fail (array_length > 0, NULL);

  tivxReferenceExportHandle ((vx_reference) exemplar,
      pyramid_addr, pyramid_size, MODULE_MAX_NUM_PYRAMIDS, &num_entries);

  /* Create new array based on exemplar */
  array = vxCreateObjectArray (vxGetContext (exemplar), exemplar, array_length);
  for (i = 0; i < array_length; i++) {
    vx_reference ref = NULL;

    ref = vxGetObjectArrayItem (array, i);
    for (j = 0; j < num_entries; j++) {
      addr[i + j] = (void *) (mem_start + prev_size);
      prev_size += pyramid_size[j];
    }

    status =
        tivxReferenceImportHandle ((vx_reference) ref, (const void **) &addr[i],
        pyramid_size, num_entries);

    if (ref != NULL) {
      vxReleaseReference ((vx_reference *) & ref);
    }

    if (status != VX_SUCCESS) {
      GST_ERROR_OBJECT (buffer,
          "Unable to import tivx_shared_mem_ptr to a vx_pyramid: %"
          G_GINT32_FORMAT, status);
      vxReleaseObjectArray (&array);
      goto out;
    }
  }

  /* Add pyramid meta to the buffer */
  tiovx_pyramid_meta =
      (GstTIOVXPyramidMeta *) gst_buffer_add_meta (buffer,
      gst_tiovx_pyramid_meta_get_info (), NULL);

  /* Retrieve pyramid info from exemplar */
  vxQueryPyramid ((vx_pyramid) exemplar, VX_PYRAMID_LEVELS,
      &levels, sizeof (levels));
  vxQueryPyramid ((vx_pyramid) exemplar, VX_PYRAMID_SCALE,
      &scale, sizeof (scale));
  vxQueryPyramid ((vx_pyramid) exemplar, VX_PYRAMID_WIDTH,
      &width, sizeof (width));
  vxQueryPyramid ((vx_pyramid) exemplar, VX_PYRAMID_HEIGHT,
      &height, sizeof (height));
  vxQueryPyramid ((vx_pyramid) exemplar, VX_PYRAMID_FORMAT,
      &vx_format, sizeof (vx_format));

  /* Fill pyramid info */
  tiovx_pyramid_meta->array = array;
  tiovx_pyramid_meta->pyramid_info.levels = levels;
  tiovx_pyramid_meta->pyramid_info.scale = scale;
  tiovx_pyramid_meta->pyramid_info.width = width;
  tiovx_pyramid_meta->pyramid_info.height = height;
  tiovx_pyramid_meta->pyramid_info.format = vx_format_to_gst_format (vx_format);
  tiovx_pyramid_meta->pyramid_info.size = channel_size;

out:
  return tiovx_pyramid_meta;
}
