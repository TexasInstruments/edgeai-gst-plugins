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

#include "gsttiovxsensormeta.h"

#include <TI/tivx.h>

#include "gsttiovxutils.h"

static gboolean gst_tiovx_sensor_meta_init (GstMeta * meta,
    gpointer params, GstBuffer * buffer);

void gst_tiovx_sensor_meta_free (GstMeta * meta, GstBuffer * buffer);

GType
gst_tiovx_sensor_meta_api_get_type (void)
{
  static volatile GType type = 0;
  static const gchar *tags[] =
      { GST_META_TAG_VIDEO_STR, GST_META_TAG_MEMORY_STR, NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstTIOVXSensorMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_tiovx_sensor_meta_get_info (void)
{
  static const GstMetaInfo *info = NULL;

  if (g_once_init_enter (&info)) {
    const GstMetaInfo *meta = gst_meta_register (GST_TYPE_TIOVX_SENSOR_META_API,
        "GstTIOVXSensorMeta",
        sizeof (GstTIOVXSensorMeta),
        gst_tiovx_sensor_meta_init,
        gst_tiovx_sensor_meta_free,
        NULL);
    g_once_init_leave (&info, meta);
  }
  return info;
}

static gboolean
gst_tiovx_sensor_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstTIOVXSensorMeta *sensor_meta = (GstTIOVXSensorMeta *) meta;

  sensor_meta->meta_before_size = 0;
  sensor_meta->meta_before = NULL;
  sensor_meta->meta_after_size = 0;
  sensor_meta->meta_after = NULL;

  return TRUE;
}

void
gst_tiovx_sensor_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstTIOVXSensorMeta *sensor_meta = (GstTIOVXSensorMeta *) meta;

  if (sensor_meta->meta_before) {
    g_free (sensor_meta->meta_before);
  }

  if (sensor_meta->meta_after) {
    g_free (sensor_meta->meta_after);
  }

  sensor_meta->meta_before_size = 0;
  sensor_meta->meta_after_size = 0;
}

GstTIOVXSensorMeta *
gst_buffer_add_tiovx_sensor_meta (GstBuffer * buffer,
    guint meta_before_size, const void *meta_before,
    guint meta_after_size, const void *meta_after)
{
  GstTIOVXSensorMeta *sensor_meta = NULL;

  g_return_val_if_fail (buffer, NULL);

  sensor_meta =
      (GstTIOVXSensorMeta *) gst_buffer_add_meta (buffer,
      gst_tiovx_sensor_meta_get_info (), NULL);

  sensor_meta->meta_before_size = meta_before_size;
  if (sensor_meta->meta_before_size) {
    sensor_meta->meta_before = g_malloc (sensor_meta->meta_before_size);
    memcpy (sensor_meta->meta_before, meta_before,
        sensor_meta->meta_before_size);
  }

  sensor_meta->meta_after_size = meta_after_size;
  if (sensor_meta->meta_after_size) {
    sensor_meta->meta_after = g_malloc (sensor_meta->meta_after_size);
    memcpy (sensor_meta->meta_after, meta_after, sensor_meta->meta_after_size);
  }

  return sensor_meta;
}
