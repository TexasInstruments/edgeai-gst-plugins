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
 * *	Any redistribution and use are licensed by TI for use only with TI Devices.
 *
 * *	Nothing shall obligate TI to provide you with source code for the
 *      software licensed and provided to you in object code.
 *
 * If software source code is provided to you, modification and redistribution
 * of the source code are permitted provided that the following conditions are met:
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/check/gstcheck.h>
#include <gst/video/video-format.h>

#include "test_utils.h"

/* Supported formats */
#define TIOVXMOSAIC_FORMATS_ARRAY_SIZE 3
static const gchar *tiovxmosaic_formats[TIOVXMOSAIC_FORMATS_ARRAY_SIZE] = {
  "NV12",
  "GRAY8",
  "GRAY16_LE",
};

/* Supported resolutions */
static const guint tiovxmosaic_width[] = { 1, 8192 };
static const guint tiovxmosaic_height[] = { 1, 8192 };

/* Supported framerate */
static const guint tiovxmosaic_framerate[] = { 1, 2147483647 };

typedef struct
{
  const guint *width;
  const guint *height;
  const guint *framerate;
  const gchar **formats;
} PadTemplateBackground;

typedef struct
{
  const guint *width;
  const guint *height;
  const guint *framerate;
  const gchar **formats;
} PadTemplateSink;

typedef struct
{
  const guint *width;
  const guint *height;
  const guint *framerate;
  const gchar **formats;
} PadTemplateSrc;

typedef struct
{
  PadTemplateBackground background_pad;
  PadTemplateSrc src_pad;
  PadTemplateSink sink_pad;
} TIOVXMosaicModeled;

static const void gst_tiovx_mosaic_modeling_init (TIOVXMosaicModeled * element);

static const void
gst_tiovx_mosaic_modeling_init (TIOVXMosaicModeled * element)
{
  element->background_pad.formats = tiovxmosaic_formats;
  element->background_pad.width = tiovxmosaic_width;
  element->background_pad.height = tiovxmosaic_height;
  element->background_pad.framerate = tiovxmosaic_framerate;

  element->sink_pad.formats = tiovxmosaic_formats;
  element->sink_pad.width = tiovxmosaic_width;
  element->sink_pad.height = tiovxmosaic_height;
  element->sink_pad.framerate = tiovxmosaic_framerate;

  element->src_pad.formats = tiovxmosaic_formats;
  element->src_pad.width = tiovxmosaic_width;
  element->src_pad.height = tiovxmosaic_height;
  element->src_pad.framerate = tiovxmosaic_framerate;
}

GST_START_TEST (test_state_change_foreach_upstream_format)
{
  TIOVXMosaicModeled element = { 0 };
  guint i = 0;

  gst_tiovx_mosaic_modeling_init (&element);

  for (i = 0; i < TIOVXMOSAIC_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) pipeline = g_string_new ("");
    g_autoptr (GString) upstream_caps = g_string_new ("");

    /* Upstream caps */
    g_string_printf (upstream_caps, "video/x-raw,format=%s",
        element.sink_pad.formats[i]);

    g_string_printf (pipeline, "videotestsrc ! %s ! tiovxmosaic ! fakesink ",
        upstream_caps->str);

    test_states_change (pipeline->str);
  }
}

GST_END_TEST;

GST_START_TEST (test_state_change_foreach_upstream_format_fail)
{
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) upstream_caps = g_string_new ("");
  g_autoptr (GString) downstream_caps = g_string_new ("");

  /* Upstream caps */
  g_string_printf (upstream_caps, "video/x-raw,format=%d",
      GST_VIDEO_FORMAT_UNKNOWN);

  g_string_printf (pipeline, "videotestsrc ! %s ! tiovxmosaic ! fakesink",
      upstream_caps->str);

  g_assert_true (NULL != test_create_pipeline_fail (pipeline->str));
}

GST_END_TEST;

GST_START_TEST (test_state_change_resolutions)
{
  TIOVXMosaicModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) upstream_caps = g_string_new ("");
  gint32 width = 0;
  gint32 height = 0;

  gst_tiovx_mosaic_modeling_init (&element);

  width =
      g_random_int_range (element.sink_pad.width[0], element.sink_pad.width[1]);
  height =
      g_random_int_range (element.sink_pad.height[0],
      element.sink_pad.height[1]);

  /* Upstream caps */
  g_string_printf (upstream_caps, "video/x-raw,width=%d,height=%d", width,
      height);

  g_string_printf (pipeline, "videotestsrc ! %s ! tiovxmosaic ! fakesink ",
      upstream_caps->str);

  test_states_change (pipeline->str);
}

GST_END_TEST;

GST_START_TEST (test_state_change_resolutions_with_upscale_fail)
{
  TIOVXMosaicModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) upstream_caps = g_string_new ("");
  g_autoptr (GString) downstream_caps = g_string_new ("");
  gint32 width = 0;
  gint32 height = 0;

  gst_tiovx_mosaic_modeling_init (&element);

  width =
      g_random_int_range (element.sink_pad.width[0], element.sink_pad.width[1]);
  height =
      g_random_int_range (element.sink_pad.height[0],
      element.sink_pad.height[1]);

  /* Upstream caps */
  g_string_printf (upstream_caps, "video/x-raw,width=%d,height=%d", width,
      height);

  /* Downstream caps */
  g_string_printf (downstream_caps, "video/x-raw,width=%d,height=%d", width + 1,
      height + 1);

  g_string_printf (pipeline, "videotestsrc ! %s ! tiovxmosaic ! %s ! fakesink ",
      upstream_caps->str, downstream_caps->str);

  test_states_change_fail (pipeline->str);
}

GST_END_TEST;

GST_START_TEST (test_state_change_resolutions_with_downscale_fail)
{
  TIOVXMosaicModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) upstream_caps = g_string_new ("");
  g_autoptr (GString) downstream_caps = g_string_new ("");
  gint32 width = 0;
  gint32 height = 0;

  gst_tiovx_mosaic_modeling_init (&element);

  width =
      g_random_int_range (element.sink_pad.width[0], element.sink_pad.width[1]);
  height =
      g_random_int_range (element.sink_pad.height[0],
      element.sink_pad.height[1]);

  /* Upstream caps */
  g_string_printf (upstream_caps, "video/x-raw,width=%d,height=%d", width,
      height);

  /* Downstream caps */
  g_string_printf (downstream_caps, "video/x-raw,width=%d,height=%d", width - 1,
      height - 1);

  g_string_printf (pipeline, "videotestsrc ! %s ! tiovxmosaic ! %s ! fakesink ",
      upstream_caps->str, downstream_caps->str);

  test_states_change_fail (pipeline->str);
}

GST_END_TEST;

static Suite *
gst_state_suite (void)
{
  Suite *suite = suite_create ("tiovxmosaic");
  TCase *tc = tcase_create ("tc");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_state_change_foreach_upstream_format);
  tcase_add_test (tc, test_state_change_foreach_upstream_format_fail);
  tcase_add_test (tc, test_state_change_resolutions);
  tcase_add_test (tc, test_state_change_resolutions_with_upscale_fail);
  tcase_add_test (tc, test_state_change_resolutions_with_downscale_fail);

  return suite;
}

GST_CHECK_MAIN (gst_state);
