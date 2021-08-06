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

#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>

#include <gst-libs/gst/tiovx/gsttiovxbufferpool.h>
#include <gst-libs/gst/tiovx/gsttiovxmiso.h>

/* Start of Dummy MISO element */

#define GST_TYPE_TEST_TIOVX_MISO            (gst_test_tiovx_miso_get_type ())
#define GST_TEST_TIOVX_MISO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_TEST_TIOVX_MISO, GstTestTIOVXMiso))
#define GST_TEST_TIOVX_MISO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_TEST_TIOVX_MISO, GstTestTIOVXMisoClass))
#define GST_TEST_TIOVX_MISO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TEST_TIOVX_MISO, GstTestTIOVXMisoClass))

typedef struct _GstTestTIOVXMiso GstTestTIOVXMiso;
typedef struct _GstTestTIOVXMisoClass GstTestTIOVXMisoClass;

static GType gst_test_tiovx_miso_get_type (void);

struct _GstTestTIOVXMiso
{
  GstTIOVXMiso parent;
};

struct _GstTestTIOVXMisoClass
{
  GstTIOVXMisoClass parent_class;
};

#define gst_test_tiovx_miso_parent_class parent_class
G_DEFINE_TYPE (GstTestTIOVXMiso, gst_test_tiovx_miso, GST_TIOVX_MISO_TYPE);

static void
gst_test_tiovx_miso_class_init (GstTestTIOVXMisoClass * klass)
{
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  static GstStaticPadTemplate _src_template =
      GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS_ANY);

  static GstStaticPadTemplate _sink_template =
      GST_STATIC_PAD_TEMPLATE ("sink_%u", GST_PAD_SINK, GST_PAD_REQUEST,
      GST_STATIC_CAPS_ANY);

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &_src_template, GST_TYPE_AGGREGATOR_PAD);

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &_sink_template, GST_TYPE_AGGREGATOR_PAD);

  gst_element_class_set_static_metadata (gstelement_class, "TIOVXMiso",
      "Testing", "Testing miso", "RidgeRun <support@ridgerun.com>");
}

static void
gst_test_tiovx_miso_init (GstTestTIOVXMiso * self)
{

}

static gboolean
gst_test_tiovx_miso_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "testtiovxmiso", GST_RANK_NONE,
      GST_TYPE_TEST_TIOVX_MISO);
}

static gboolean
gst_test_tiovx_miso_plugin_register (void)
{
  return gst_plugin_register_static (GST_VERSION_MAJOR,
      GST_VERSION_MINOR,
      "testtiovxmiso",
      "Combine buffers",
      gst_test_tiovx_miso_plugin_init,
      VERSION, GST_LICENSE_UNKNOWN, PACKAGE, GST_PACKAGE_NAME,
      GST_PACKAGE_ORIGIN);
}

/* End of Dummy SIMO element */

GST_START_TEST (test_create)
{
  GstElement *dummy_miso = NULL;
  GstHarness *h = NULL;
  GstBuffer *in_buf = NULL;
  GstBuffer *out_buf = NULL;

  dummy_miso = gst_element_factory_make ("testtiovxmiso", NULL);
  h = gst_harness_new_with_element (dummy_miso, "sink_%u", "src");
  fail_if (NULL == h, "Unable to create Test TIOVXMiso harness");

  /* we must specify a caps before pushing buffers */
  gst_harness_set_src_caps_str (h,
      "video/x-raw, format=RGBx, width=320, height=240");
  gst_harness_set_sink_caps_str (h,
      "video/x-raw, format=RGBx, width=320, height=240");

  /* create a buffer of size 42 */
  in_buf = gst_harness_create_buffer (h, 42);

  /* push the buffer into the queue */
  gst_harness_push (h, in_buf);

  /* pull the buffer from the queue */
  out_buf = gst_harness_pull (h);

  fail_if (out_buf == NULL);
  fail_if (!GST_TIOVX_IS_BUFFER_POOL (out_buf->pool));

  /* cleanup */
  gst_harness_teardown (h);
  gst_object_unref (dummy_miso);
}

GST_END_TEST;

static Suite *
gst_state_suite (void)
{
  Suite *suite;
  TCase *tc;

  gst_test_tiovx_miso_plugin_register ();

  suite = suite_create ("tiovxmiso");
  tc = tcase_create ("tc");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_create);

  return suite;
}

GST_CHECK_MAIN (gst_state);
