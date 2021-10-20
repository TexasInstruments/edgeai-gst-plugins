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

#define TIOVXDLCOLORBLEND_STATES_CHANGE_ITERATIONS 5

/* Supported formats */
#define TIOVXDLCOLORBLEND_FORMATS_ARRAY_SIZE 2
static const gchar
    * tiovxdlcolorblend_formats[TIOVXDLCOLORBLEND_FORMATS_ARRAY_SIZE] = {
  "RGB",
  "NV12",
};

/* Supported dimensions.
 * FIXME: TIOVX nodes doesn't support high input resolution for the moment.
*/
static const guint tiovxdlcolorblend_width[] = { 1, 2048 };
static const guint tiovxdlcolorblend_height[] = { 1, 1080 };

/* Supported data-type */
#define TIOVXDLCOLORBLEND_DATA_TYPE_ARRAY_SIZE 7
static const gchar
    * tiovxdlcolorblend_data_type[TIOVXDLCOLORBLEND_DATA_TYPE_ARRAY_SIZE] = {
  "int8",
  "uint8",
  "int16",
  "uint16",
  "int32",
  "uint32",
  "float32",
};

/* Supported targets */
#define TIOVXDLCOLORBLEND_TARGETS_ARRAY_SIZE 7
static const gchar
    * tiovxdlcolorblend_targets[TIOVXDLCOLORBLEND_TARGETS_ARRAY_SIZE] = {
  "DSP-1",
  "DSP-2",
};

typedef struct
{
  const guint *width;
  const guint *height;
  const gchar **formats;
} PadTemplateSink;

typedef struct
{
  const guint *width;
  const guint *height;
  const gchar **formats;
} PadTemplateSrc;

typedef struct
{
  const guint *width;
  const guint *height;
  const gchar **data_type;
} PadTemplateTensor;

typedef struct
{
  PadTemplateSrc src_pad;
  PadTemplateSink sink_pad;
  PadTemplateTensor tensor_pad;

  const gchar **target;
} TIOVXDLColorBlendModeled;

static const void
gst_tiovx_dl_color_blend_modeling_init (TIOVXDLColorBlendModeled * element);

static const void
gst_tiovx_dl_color_blend_modeling_init (TIOVXDLColorBlendModeled * element)
{
  element->sink_pad.formats = tiovxdlcolorblend_formats;
  element->sink_pad.width = tiovxdlcolorblend_width;
  element->sink_pad.height = tiovxdlcolorblend_height;

  element->tensor_pad.data_type = tiovxdlcolorblend_data_type;
  element->tensor_pad.width = tiovxdlcolorblend_width;
  element->tensor_pad.height = tiovxdlcolorblend_height;

  element->target = tiovxdlcolorblend_targets;
}

GST_START_TEST (test_dummy)
{
  TIOVXDLColorBlendModeled element = { 0 };

  gst_tiovx_dl_color_blend_modeling_init (&element);

  g_assert_true (0 == g_strcmp0 ("RGB", element.sink_pad.formats[0]));
}

GST_END_TEST;

static Suite *
gst_state_suite (void)
{
  Suite *suite = suite_create ("tiovxdlcolorblend");
  TCase *tc = tcase_create ("tc");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_dummy);

  return suite;
}

GST_CHECK_MAIN (gst_state);
