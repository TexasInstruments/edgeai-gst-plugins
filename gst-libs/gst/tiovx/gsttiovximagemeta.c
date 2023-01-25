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

#include "gsttiovximagemeta.h"

#include <TI/tivx.h>

#include "gsttiovxutils.h"

static gboolean gst_tiovx_image_meta_init (GstMeta * meta,
    gpointer params, GstBuffer * buffer);

GType
gst_tiovx_image_meta_api_get_type (void)
{
  static volatile GType type = 0;
  static const gchar *tags[] =
      { GST_META_TAG_VIDEO_STR, GST_META_TAG_MEMORY_STR, NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstTIOVXImageMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_tiovx_image_meta_get_info (void)
{
  static const GstMetaInfo *info = NULL;

  if (g_once_init_enter (&info)) {
    const GstMetaInfo *meta = gst_meta_register (GST_TYPE_TIOVX_IMAGE_META_API,
        "GstTIOVXImageMeta",
        sizeof (GstTIOVXImageMeta),
        gst_tiovx_image_meta_init,
        NULL,
        NULL);
    g_once_init_leave (&info, meta);
  }
  return info;
}

static gboolean
gst_tiovx_image_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  /* Gst requires this func to be implemented, even if it is empty */
  return TRUE;
}

static void
gst_tiovx_image_meta_get_plane_info (const vx_image image,
    const gint plane_index, gint * plane_stride_x, gint * plane_stride_y,
    gint * plane_step_x, gint * plane_step_y, gint * plane_width,
    gint * plane_height)
{
  vx_imagepatch_addressing_t img_addr[MODULE_MAX_NUM_PLANES];

  g_return_if_fail (NULL != image);
  g_return_if_fail (NULL != plane_stride_x);
  g_return_if_fail (NULL != plane_stride_y);
  g_return_if_fail (NULL != plane_step_x);
  g_return_if_fail (NULL != plane_width);
  g_return_if_fail (NULL != plane_height);

  vxQueryImage (image, TIVX_IMAGE_IMAGEPATCH_ADDRESSING, img_addr, sizeof (img_addr));

  *plane_stride_x = img_addr[plane_index].stride_x;
  *plane_stride_y = img_addr[plane_index].stride_y;
  *plane_step_x = img_addr[plane_index].step_x;
  *plane_step_y = img_addr[plane_index].step_y;
  *plane_width = img_addr[plane_index].dim_x;
  *plane_height = img_addr[plane_index].dim_y;
}

GstTIOVXImageMeta *
gst_buffer_add_tiovx_image_meta (GstBuffer * buffer,
    const vx_reference exemplar, const gint array_length, guint64 mem_start)
{
  GstTIOVXImageMeta *tiovx_meta = NULL;
  void *addr[MODULE_MAX_NUM_PLANES] = { NULL };
  void *plane_addr[MODULE_MAX_NUM_PLANES] = { NULL };
  gsize plane_offset[MODULE_MAX_NUM_PLANES] = { 0 };
  gint plane_stride_x[MODULE_MAX_NUM_PLANES] = { 0 };
  gint plane_stride_y[MODULE_MAX_NUM_PLANES] = { 0 };
  gint plane_steps_x[MODULE_MAX_NUM_PLANES] = { 0 };
  gint plane_steps_y[MODULE_MAX_NUM_PLANES] = { 0 };
  gint plane_widths[MODULE_MAX_NUM_PLANES] = { 0 };
  gint plane_heights[MODULE_MAX_NUM_PLANES] = { 0 };
  vx_uint32 plane_sizes[MODULE_MAX_NUM_PLANES];
  guint num_planes = 0;
  guint plane_idx = 0;
  gint prev_size = 0;
  gint i = 0;
  vx_object_array array;
  vx_image ref = NULL;
  vx_df_image vx_format = VX_DF_IMAGE_VIRT;
  vx_status status;

  g_return_val_if_fail (buffer, NULL);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) exemplar),
      NULL);
  g_return_val_if_fail (array_length > 0, NULL);

  if (NULL == buffer) {
    GST_ERROR_OBJECT (buffer, "Cannot add meta, invalid buffer pointer");
    goto out;
  }

  if (VX_SUCCESS != vxGetStatus ((vx_reference) exemplar)) {
    GST_ERROR_OBJECT (buffer, "Cannot add meta, invalid exemplar reference");
    goto out;
  }

  /* Get plane and size information */
  tivxReferenceExportHandle ((vx_reference) exemplar,
      plane_addr, plane_sizes, MODULE_MAX_NUM_PLANES, &num_planes);

  array = vxCreateObjectArray (vxGetContext (exemplar), exemplar, array_length);

  for (i = 0; i < array_length; i++) {
    for (plane_idx = 0; plane_idx < num_planes; plane_idx++) {
      addr[plane_idx] = (void *) (mem_start + prev_size);
      plane_offset[plane_idx] = prev_size;

      gst_tiovx_image_meta_get_plane_info ((vx_image) exemplar, plane_idx,
          &plane_stride_x[plane_idx], &plane_stride_y[plane_idx],
          &plane_steps_x[plane_idx], &plane_steps_y[plane_idx],
          &plane_widths[plane_idx], &plane_heights[plane_idx]);

      prev_size += plane_sizes[plane_idx];
    }

    /* Import memory into the meta's vx reference */
    ref = (vx_image) vxGetObjectArrayItem (array, i);
    status =
        tivxReferenceImportHandle ((vx_reference) ref, (const void **) addr,
        plane_sizes, num_planes);

    if (ref != NULL) {
      vxReleaseReference ((vx_reference *) & ref);
    }
    if (status != VX_SUCCESS) {
      GST_ERROR_OBJECT (buffer,
          "Unable to import tivx_shared_mem_ptr to a vx_image: %"
          G_GINT32_FORMAT, status);
      goto err_out;
    }
  }

  tiovx_meta =
      (GstTIOVXImageMeta *) gst_buffer_add_meta (buffer,
      gst_tiovx_image_meta_get_info (), NULL);
  tiovx_meta->array = array;

  /* Update information for the VideoMeta, since all buffers are the same,
   * we can use the last update data
   */
  tiovx_meta->image_info.num_planes = num_planes;
  for (plane_idx = 0; plane_idx < num_planes; plane_idx++) {
    tiovx_meta->image_info.plane_offset[plane_idx] = plane_offset[plane_idx];
    tiovx_meta->image_info.plane_stride_x[plane_idx] =
        plane_stride_x[plane_idx];
    tiovx_meta->image_info.plane_stride_y[plane_idx] =
        plane_stride_y[plane_idx];
    tiovx_meta->image_info.plane_sizes[plane_idx] = plane_sizes[plane_idx];
    tiovx_meta->image_info.plane_steps_x[plane_idx] = plane_steps_x[plane_idx];
    tiovx_meta->image_info.plane_steps_y[plane_idx] = plane_steps_y[plane_idx];
    tiovx_meta->image_info.plane_widths[plane_idx] = plane_widths[plane_idx];
    tiovx_meta->image_info.plane_heights[plane_idx] = plane_heights[plane_idx];
  }

  /* Retrieve width, height and format from exemplar */
  vxQueryImage ((vx_image) exemplar, VX_IMAGE_WIDTH,
      &tiovx_meta->image_info.width, sizeof (tiovx_meta->image_info.width));
  vxQueryImage ((vx_image) exemplar, VX_IMAGE_HEIGHT,
      &tiovx_meta->image_info.height, sizeof (tiovx_meta->image_info.height));
  vxQueryImage ((vx_image) exemplar, VX_IMAGE_FORMAT, &vx_format,
      sizeof (vx_format));
  tiovx_meta->image_info.format = vx_format_to_gst_format (vx_format);

  goto out;

err_out:
  vxReleaseObjectArray (&array);
out:
  return tiovx_meta;
}
