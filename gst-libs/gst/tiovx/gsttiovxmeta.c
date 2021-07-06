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

#include <gst/video/video.h>
#include <TI/tivx.h>

#include "gsttiovxmeta.h"

static gboolean gst_tiovx_meta_init (GstMeta * meta,
    gpointer params, GstBuffer * buffer);
static void gst_tiovx_meta_set_qdata (GstBuffer * buffer, gpointer data);

#define VX_IMAGE_QUARK_STR "VXImage"
static GQuark _vx_image_quark;

static void
gst_tiovx_meta_set_qdata (GstBuffer * buffer, gpointer data)
{
  GstMemory *mem = NULL;

  mem = gst_buffer_get_all_memory (buffer);
  gst_mini_object_set_qdata (GST_MINI_OBJECT_CAST (mem), _vx_image_quark, data, NULL);  // TODO delete function should be generic? who deletes vx_image?
  gst_memory_unref (mem);
}

GType
gst_tiovx_meta_api_get_type (void)
{
  static volatile GType type = 0;
  static const gchar *tags[] = { GST_META_TAG_VIDEO_STR, NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstTIOVXMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_tiovx_meta_get_info (void)
{
  static const GstMetaInfo *info = NULL;

  if (g_once_init_enter (&info)) {
    const GstMetaInfo *meta = gst_meta_register (GST_TIOVX_META_API_TYPE,
        "GstTIOVXMeta",
        sizeof (GstTIOVXMeta),
        gst_tiovx_meta_init,
        NULL,
        NULL);
    g_once_init_leave (&info, meta);
  }
  return info;
}

GstTIOVXMeta *
gst_tiovx_buffer_add_meta_vx_image (GstBuffer * buffer, vx_image * image)
{
  g_return_val_if_fail (buffer != NULL, NULL);
  g_return_val_if_fail (image != NULL, NULL);

  GST_LOG ("Adding TIOVX meta to buffer %p", buffer);

  gst_tiovx_meta_set_qdata (buffer, image);

  return (GstTIOVXMeta *) gst_buffer_add_meta (buffer, GST_TIOVX_META_INFO,
      NULL);
}

static gboolean
gst_tiovx_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  /* Gst requieres this func to be implemented, even if it is empty */
  return TRUE;
}
