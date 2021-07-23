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

#ifndef HOST_EMULATION
#define HOST_EMULATION 0
#endif

#include "ext/tiovx/gsttiovxmultiscaler.h"

#include <app_scaler_module.h>
#include <gst/check/gstharness.h>
#include <gst/check/gstcheck.h>
#include <gst/video/video-format.h>

#include <gst-libs/gst/tiovx/gsttiovxallocator.h>
#include <gst-libs/gst/tiovx/gsttiovxbufferpool.h>
#include <gst-libs/gst/tiovx/gsttiovxmeta.h>

#include "app_scaler_module.h"
#include "gst-libs/gst/tiovx/gsttiovxsimo.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"
#include "test_utils.h"

#include "app_init.h"

#define DEFAULT_MIN_NUM_OUTPUTS 1

#define MIN_POOL_SIZE 2
#define MAX_POOL_SIZE 16

static const gchar *test_pipelines[] = {
  /* start pipeline */
  "videotestsrc ! tiovxmultiscaler ! fakesink",
  NULL,
};

enum
{
  /* Pipelines names */
  TEST_PLAYING_TO_NULL_MULTIPLE_TIMES,
};

GST_START_TEST (test_playing_to_null_multiple_times)
{
  test_states_change (test_pipelines[TEST_PLAYING_TO_NULL_MULTIPLE_TIMES]);
}

GST_END_TEST;

GST_START_TEST (test_init_module)
{
  GstElement *element = NULL;
  GstTIOVXSimoClass *simo_class = NULL;
  GstTIOVXSimo *simo = NULL;
  vx_context context = NULL;
  guint in_pool_size = 0;
  GHashTable *out_pool_sizes = NULL;
  vx_status status = VX_SUCCESS;
  gint ret = 0;
  guint i = 0;
  gint num_pads = 0;
  GstCaps *in_caps = NULL;
  GstCaps *out_caps = NULL;
  GList *src_caps_list = NULL;

  /* Setup */
  gst_init (NULL, NULL);
  element = gst_element_factory_make ("tiovxmultiscaler", "tiovxmultiscaler");

  simo_class = GST_TIOVX_SIMO_GET_CLASS (element);
  simo = GST_TIOVX_SIMO (element);
  in_pool_size = g_random_int_range (MIN_POOL_SIZE, MAX_POOL_SIZE);
  out_pool_sizes = g_hash_table_new (NULL, NULL);

  num_pads = gst_tiovx_simo_get_num_pads (simo);

  in_caps =
      gst_caps_from_string ("video/x-raw,width=1920,height=1080,format=NV12");
  out_caps =
      gst_caps_from_string ("video/x-raw,width=2040,height=1920,format=NV12");

  for (i = 0; i < num_pads; i++) {
    g_hash_table_insert (out_pool_sizes,
        GUINT_TO_POINTER (i), GUINT_TO_POINTER (MIN_POOL_SIZE));

    src_caps_list = g_list_append (src_caps_list, out_caps);
  }
  ret = appCommonInit ();
  g_assert_true (0 == ret);

  tivxInit ();
  tivxHostInit ();
  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  g_assert_true (VX_SUCCESS == status);

  g_assert_true (GST_IS_TIOVX_SIMO_CLASS (simo_class));
  g_assert_true (GST_IS_TIOVX_SIMO (simo));

  /* Test the module init */
  ret =
      simo_class->init_module (simo, context, in_caps, src_caps_list,
      in_pool_size, out_pool_sizes);
  g_assert_true (ret);

  /* Teardown */
  vxReleaseContext (&context);
  appCommonDeInit ();

  g_hash_table_unref (out_pool_sizes);

  g_list_free (src_caps_list);

  gst_caps_unref (in_caps);
  gst_caps_unref (out_caps);
}

GST_END_TEST;

GST_START_TEST (test_deinit_module)
{
  GstElement *element = NULL;
  GstTIOVXSimoClass *simo_class = NULL;
  GstTIOVXSimo *simo = NULL;
  vx_context context = NULL;
  guint in_pool_size = 0;
  GHashTable *out_pool_sizes = NULL;
  vx_status status = VX_SUCCESS;
  gint ret = 0;
  guint i = 0;
  gint num_pads = 0;
  GstCaps *in_caps = NULL;
  GstCaps *out_caps = NULL;
  GList *src_caps_list = NULL;

  /* Setup */
  gst_init (NULL, NULL);
  element = gst_element_factory_make ("tiovxmultiscaler", "tiovxmultiscaler");

  simo_class = GST_TIOVX_SIMO_GET_CLASS (element);
  simo = GST_TIOVX_SIMO (element);
  in_pool_size = g_random_int_range (MIN_POOL_SIZE, MAX_POOL_SIZE);
  out_pool_sizes = g_hash_table_new (NULL, NULL);

  num_pads = gst_tiovx_simo_get_num_pads (simo);

  in_caps =
      gst_caps_from_string ("video/x-raw,width=1920,height=1080,format=NV12");
  out_caps =
      gst_caps_from_string ("video/x-raw,width=2040,height=1920,format=NV12");

  for (i = 0; i < num_pads; i++) {
    g_hash_table_insert (out_pool_sizes,
        GUINT_TO_POINTER (i), GUINT_TO_POINTER (MIN_POOL_SIZE));

    src_caps_list = g_list_append (src_caps_list, out_caps);
  }

  ret = appCommonInit ();
  g_assert_true (0 == ret);

  tivxInit ();
  tivxHostInit ();
  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  g_assert_true (VX_SUCCESS == status);

  g_assert_true (GST_IS_TIOVX_SIMO_CLASS (simo_class));
  g_assert_true (GST_IS_TIOVX_SIMO (simo));

  ret =
      simo_class->init_module (simo, context, in_caps, src_caps_list,
      in_pool_size, out_pool_sizes);
  g_assert_true (ret);

  /* Test the module deinit */
  ret = simo_class->deinit_module (simo);
  g_assert_true (ret);

  /* Teardown */
  vxReleaseContext (&context);
  appCommonDeInit ();

  g_hash_table_unref (out_pool_sizes);

  g_list_free (src_caps_list);

  gst_caps_unref (in_caps);
  gst_caps_unref (out_caps);
}

GST_END_TEST;

GST_START_TEST (test_configure_module)
{
  GstElement *element = NULL;
  GstTIOVXSimoClass *simo_class = NULL;
  GstTIOVXSimo *simo = NULL;
  vx_context context = NULL;
  guint in_pool_size = 0;
  GHashTable *out_pool_sizes = NULL;
  vx_status status = VX_SUCCESS;
  gint ret = 0;
  guint i = 0;
  gint num_pads = 0;
  GstCaps *in_caps = NULL;
  GstCaps *out_caps = NULL;
  GList *src_caps_list = NULL;
  vx_graph graph = NULL;

  /* Setup */
  gst_init (NULL, NULL);
  element = gst_element_factory_make ("tiovxmultiscaler", "tiovxmultiscaler");

  simo_class = GST_TIOVX_SIMO_GET_CLASS (element);
  simo = GST_TIOVX_SIMO (element);
  in_pool_size = g_random_int_range (MIN_POOL_SIZE, MAX_POOL_SIZE);
  out_pool_sizes = g_hash_table_new (NULL, NULL);

  num_pads = gst_tiovx_simo_get_num_pads (simo);

  in_caps =
      gst_caps_from_string ("video/x-raw,width=1920,height=1080,format=NV12");
  out_caps =
      gst_caps_from_string ("video/x-raw,width=2040,height=1920,format=NV12");

  for (i = 0; i < num_pads; i++) {
    g_hash_table_insert (out_pool_sizes,
        GUINT_TO_POINTER (i), GUINT_TO_POINTER (MIN_POOL_SIZE));

    src_caps_list = g_list_append (src_caps_list, out_caps);
  }

  ret = appCommonInit ();
  g_assert_true (0 == ret);

  tivxInit ();
  tivxHostInit ();
  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  g_assert_true (VX_SUCCESS == status);

  tivxHwaLoadKernels (context);

  g_assert_true (GST_IS_TIOVX_SIMO_CLASS (simo_class));
  g_assert_true (GST_IS_TIOVX_SIMO (simo));

  ret =
      simo_class->init_module (simo, context, in_caps, src_caps_list,
      in_pool_size, out_pool_sizes);
  g_assert_true (ret);

  ret = simo_class->create_graph (simo, context, graph);
  g_assert_true (ret);

  /* Test the module config */
  ret = simo_class->configure_module (simo);
  g_assert_true (ret);

  /* Teardown */
  vxReleaseContext (&context);
  appCommonDeInit ();

  g_hash_table_unref (out_pool_sizes);

  g_list_free (src_caps_list);

  gst_caps_unref (in_caps);
  gst_caps_unref (out_caps);
}

GST_END_TEST;

GST_START_TEST (test_create_graph)
{
  GstElement *element = NULL;
  GstTIOVXSimoClass *simo_class = NULL;
  GstTIOVXSimo *simo = NULL;
  vx_context context = NULL;
  guint in_pool_size = 0;
  GHashTable *out_pool_sizes = NULL;
  vx_status status = VX_SUCCESS;
  gint ret = 0;
  guint i = 0;
  gint num_pads = 0;
  GstCaps *in_caps = NULL;
  GstCaps *out_caps = NULL;
  GList *src_caps_list = NULL;
  vx_graph graph = NULL;

  /* Setup */
  gst_init (NULL, NULL);
  element = gst_element_factory_make ("tiovxmultiscaler", "tiovxmultiscaler");

  simo_class = GST_TIOVX_SIMO_GET_CLASS (element);
  simo = GST_TIOVX_SIMO (element);
  in_pool_size = g_random_int_range (MIN_POOL_SIZE, MAX_POOL_SIZE);
  out_pool_sizes = g_hash_table_new (NULL, NULL);

  num_pads = gst_tiovx_simo_get_num_pads (simo);

  in_caps =
      gst_caps_from_string ("video/x-raw,width=1920,height=1080,format=NV12");
  out_caps =
      gst_caps_from_string ("video/x-raw,width=2040,height=1920,format=NV12");

  for (i = 0; i < num_pads; i++) {
    g_hash_table_insert (out_pool_sizes,
        GUINT_TO_POINTER (i), GUINT_TO_POINTER (MIN_POOL_SIZE));

    src_caps_list = g_list_append (src_caps_list, out_caps);
  }

  ret = appCommonInit ();
  g_assert_true (0 == ret);

  tivxInit ();
  tivxHostInit ();
  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  g_assert_true (VX_SUCCESS == status);

  tivxHwaLoadKernels (context);

  g_assert_true (GST_IS_TIOVX_SIMO_CLASS (simo_class));
  g_assert_true (GST_IS_TIOVX_SIMO (simo));

  ret =
      simo_class->init_module (simo, context, in_caps, src_caps_list,
      in_pool_size, out_pool_sizes);
  g_assert_true (ret);

  /* Test the graph */
  ret = simo_class->create_graph (simo, context, graph);
  g_assert_true (ret);

  /* Teardown */
  vxReleaseContext (&context);
  appCommonDeInit ();

  g_hash_table_unref (out_pool_sizes);

  g_list_free (src_caps_list);

  gst_caps_unref (in_caps);
  gst_caps_unref (out_caps);
}

GST_END_TEST;

GST_START_TEST (test_create_gstharness)
{
  GstHarness *h = NULL;

  h = gst_harness_new ("tiovxmultiscaler");

  g_assert_true (NULL != h);
}

GST_END_TEST;

GST_START_TEST (test_get_caps)
{
  GstElement *element = NULL;
  GstTIOVXSimoClass *simo_class = NULL;
  GstTIOVXSimo *simo = NULL;
  GList *src_caps_list = NULL;
  guint i = 0;
  guint num_pads = 0;
  GstCaps *caps = NULL;
  GstCaps *simo_caps = NULL;
  gboolean ret = 0;

  element = gst_element_factory_make ("tiovxmultiscaler", "tiovxmultiscaler");

  simo_class = GST_TIOVX_SIMO_GET_CLASS (element);
  simo = GST_TIOVX_SIMO (element);

  src_caps_list = g_list_alloc ();

  num_pads = gst_tiovx_simo_get_num_pads (simo);

  caps =
      gst_caps_from_string ("video/x-raw,width=2040,height=1920,format=NV12");

  src_caps_list = src_caps_list->prev;
  for (i = 0; i < num_pads; i++) {
    src_caps_list = g_list_insert (src_caps_list, caps, -1);
  }

  /* Test the get caps */
  simo_caps = simo_class->get_caps (simo, NULL, src_caps_list);

  {
    GstStructure *structure = NULL;
    structure = gst_caps_get_structure (simo_caps, 0);
    g_assert_true (!gst_structure_has_field (structure, "width"));
  }

  ret = GST_IS_CAPS (simo_caps);
  g_assert_true (ret);

  gst_caps_unref (caps);
  g_list_free (src_caps_list);
}

GST_END_TEST;

static Suite *
gst_state_suite (void)
{
  Suite *suite = suite_create ("tiovxmultiscaler");
  TCase *sucess_escenario = tcase_create ("sucess_escenario");

  suite_add_tcase (suite, sucess_escenario);
  tcase_skip_broken_test (sucess_escenario,
      test_playing_to_null_multiple_times);
  tcase_add_test (sucess_escenario, test_init_module);
  tcase_add_test (sucess_escenario, test_deinit_module);
  tcase_skip_broken_test (sucess_escenario, test_create_graph);
  tcase_skip_broken_test (sucess_escenario, test_configure_module);
  tcase_skip_broken_test (sucess_escenario, test_create_gstharness);

  tcase_add_test (sucess_escenario, test_get_caps);
  return suite;
}

GST_CHECK_MAIN (gst_state);
