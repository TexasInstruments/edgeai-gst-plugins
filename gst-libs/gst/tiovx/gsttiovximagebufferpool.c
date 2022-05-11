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
 * *    No reverse engineering, decompilation, or disassembly of this software
 *      is permitted with respect to any software provided in binary form.
 *
 * *    Any redistribution and use are licensed by TI for use only with TI Devices.
 *
 * *    Nothing shall obligate TI to provide you with source code for the
 *      software licensed and provided to you in object code.
 *
 * If software source code is provided to you, modification and redistribution
 * of the source code are permitted provided that the following conditions are met:
 *
 * *    Any redistribution and use of the source code, including any resulting
 *      derivative works, are licensed by TI for use only with TI Devices.
 *
 * *    Any redistribution and use of any object code compiled from the source
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsttiovximagebufferpool.h"
#include "gsttiovximagemeta.h"
#include "gsttiovxutils.h"

/* TIOVX ImageBufferPool */

struct _GstTIOVXImageBufferPool
{
  GstTIOVXBufferPool parent;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_image_buffer_pool_debug);
#define GST_CAT_DEFAULT gst_tiovx_image_buffer_pool_debug

#define gst_tiovx_image_buffer_pool_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXImageBufferPool, gst_tiovx_image_buffer_pool,
    GST_TYPE_TIOVX_BUFFER_POOL,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_image_buffer_pool_debug,
        "tiovximagebufferpool", 0,
        "debug category for the tiovximagebufferpool element"));

static gboolean gst_tiovx_image_buffer_pool_validate_caps (GstTIOVXBufferPool *
    self, const GstCaps * caps, const vx_reference exemplar);
static void gst_tiovx_image_buffer_pool_add_meta_to_buffer (GstTIOVXBufferPool *
    self, GstBuffer * buffer, vx_reference reference, guint num_channels,
    GstTIOVXMemoryData * ti_memory);
static void gst_tiovx_image_buffer_pool_free_buffer_meta (GstTIOVXBufferPool *
    self, GstBuffer * buffer);

static void
gst_tiovx_image_buffer_pool_class_init (GstTIOVXImageBufferPoolClass * klass)
{
  GstTIOVXBufferPoolClass *gsttiovxbufferpool_class = NULL;

  gsttiovxbufferpool_class = GST_TIOVX_BUFFER_POOL_CLASS (klass);

  gsttiovxbufferpool_class->validate_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_image_buffer_pool_validate_caps);
  gsttiovxbufferpool_class->add_meta_to_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_image_buffer_pool_add_meta_to_buffer);
  gsttiovxbufferpool_class->free_buffer_meta =
      GST_DEBUG_FUNCPTR (gst_tiovx_image_buffer_pool_free_buffer_meta);
}

static void
gst_tiovx_image_buffer_pool_init (GstTIOVXImageBufferPool * self)
{
}

static gboolean
gst_tiovx_image_buffer_pool_validate_caps (GstTIOVXBufferPool * self,
    const GstCaps * caps, const vx_reference exemplar)
{
  vx_df_image vx_format = VX_DF_IMAGE_VIRT;
  vx_size img_size = 0;
  guint img_width = 0, img_height = 0;
  gboolean ret = FALSE;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (caps, FALSE);
  g_return_val_if_fail (exemplar, FALSE);

  vxQueryImage ((vx_image) exemplar, VX_IMAGE_WIDTH, &img_width,
      sizeof (img_width));
  vxQueryImage ((vx_image) exemplar, VX_IMAGE_HEIGHT, &img_height,
      sizeof (img_height));
  vxQueryImage ((vx_image) exemplar, VX_IMAGE_FORMAT, &vx_format,
      sizeof (vx_format));
  vxQueryImage ((vx_image) exemplar, VX_IMAGE_SIZE, &img_size,
      sizeof (img_size));

  if (gst_structure_has_name (gst_caps_get_structure (caps, 0), "video/x-raw")
      || gst_structure_has_name (gst_caps_get_structure (caps, 0),
          "video/x-raw(" GST_CAPS_FEATURE_BATCHED_MEMORY ")")) {
    GstVideoInfo video_info;

    if (!gst_video_info_from_caps (&video_info, caps)) {
      GST_ERROR_OBJECT (self, "Unable to parse caps info");
      goto out;
    }

    if (img_width != video_info.width) {
      GST_ERROR_OBJECT (self, "Exemplar and caps's width don't match");
      goto out;
    }

    if (img_height != video_info.height) {
      GST_ERROR_OBJECT (self, "Exemplar and caps's height don't match");
      goto out;
    }

    if (vx_format_to_gst_format (vx_format) != video_info.finfo->format) {
      GST_ERROR_OBJECT (self, "Exemplar and caps's format don't match");
      goto out;
    }
  } else if (gst_structure_has_name (gst_caps_get_structure (caps, 0),
          "application/x-dof-tiovx")
      || gst_structure_has_name (gst_caps_get_structure (caps, 0),
          "application/x-dof-tiovx(" GST_CAPS_FEATURE_BATCHED_MEMORY ")")) {
    gint width = 0;
    gint height = 0;

    if (!gst_structure_get_int (gst_caps_get_structure (caps, 0),
            "width", &width)) {
      GST_ERROR_OBJECT (self, "width not found in dof caps");
      goto out;
    }

    if (!gst_structure_get_int (gst_caps_get_structure (caps, 0),
            "height", &height)) {
      GST_ERROR_OBJECT (self, "height not found in dof caps");
      goto out;
    }

    if (img_width != width) {
      GST_ERROR_OBJECT (self, "Exemplar and caps's width don't match");
      goto out;
    }

    if (img_height != height) {
      GST_ERROR_OBJECT (self, "Exemplar and caps's height don't match");
      goto out;
    }
  } else if (gst_structure_has_name (gst_caps_get_structure (caps, 0),
          "application/x-sde-tiovx")
      || gst_structure_has_name (gst_caps_get_structure (caps, 0),
          "application/x-sde-tiovx(" GST_CAPS_FEATURE_BATCHED_MEMORY ")")) {
    gint width = 0;
    gint height = 0;

    if (!gst_structure_get_int (gst_caps_get_structure (caps, 0),
            "width", &width)) {
      GST_ERROR_OBJECT (self, "width not found in sde caps");
      goto out;
    }

    if (!gst_structure_get_int (gst_caps_get_structure (caps, 0),
            "height", &height)) {
      GST_ERROR_OBJECT (self, "height not found in sde caps");
      goto out;
    }

    if (img_width != width) {
      GST_ERROR_OBJECT (self, "Exemplar and caps's width don't match");
      goto out;
    }

    if (img_height != height) {
      GST_ERROR_OBJECT (self, "Exemplar and caps's height don't match");
      goto out;
    }
  }

  ret = TRUE;

out:
  return ret;
}

void
gst_tiovx_image_buffer_pool_add_meta_to_buffer (GstTIOVXBufferPool * self,
    GstBuffer * buffer, vx_reference exemplar, guint num_channels,
    GstTIOVXMemoryData * ti_memory)
{
  GstVideoFrameFlags flags = GST_VIDEO_FRAME_FLAG_NONE;
  GstTIOVXImageMeta *tiovxmeta = NULL;

  g_return_if_fail (self);
  g_return_if_fail (buffer);
  g_return_if_fail (exemplar);
  g_return_if_fail (num_channels >= MIN_NUM_CHANNELS);
  g_return_if_fail (num_channels <= MAX_NUM_CHANNELS);
  g_return_if_fail (ti_memory);

  tiovxmeta =
      gst_buffer_add_tiovx_image_meta (buffer, exemplar, num_channels,
      ti_memory->mem_ptr.host_ptr);

  gst_buffer_add_video_meta_full (buffer,
      flags,
      tiovxmeta->image_info.format, tiovxmeta->image_info.width,
      tiovxmeta->image_info.height, tiovxmeta->image_info.num_planes,
      tiovxmeta->image_info.plane_offset, tiovxmeta->image_info.plane_stride_y);
}

void
gst_tiovx_image_buffer_pool_free_buffer_meta (GstTIOVXBufferPool * self,
    GstBuffer * buffer)
{
  GstTIOVXImageMeta *tiovxmeta = NULL;
  vx_reference ref = NULL;

  g_return_if_fail (self);
  g_return_if_fail (buffer);

  tiovxmeta =
      (GstTIOVXImageMeta *) gst_buffer_get_meta (buffer,
      GST_TYPE_TIOVX_IMAGE_META_API);
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
