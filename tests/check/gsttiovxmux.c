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
#include <gst/check/gstharness.h>

#include <gst-libs/gst/tiovx/gsttiovxallocator.h>
#include <gst-libs/gst/tiovx/gsttiovxmuxmeta.h>
#include <gst-libs/gst/tiovx/gsttiovxutils.h>

static const int kImageWidth = 320;
static const int kImageHeight = 240;
#define module_max_num_addr 4

static void
initialize_harness_and_element (GstHarness * h[], guint num_inputs)
{
  gint i = 0;
  fail_if (num_inputs == 0);

  /* The last element in h[] will be the output harness */
  h[num_inputs] = gst_harness_new_with_padnames ("tiovxmux", NULL, "src");
  fail_if (NULL == h[num_inputs], "Unable to create Test TIOVXMux harness");

  /* we must specify a caps before pushing buffers */
  gst_harness_set_sink_caps_str (h[num_inputs],
      "video/x-raw(memory:batched), format=RGBx, width=320, height=240");

  for (i = 0; i < num_inputs; i++) {
    char sink_name[80];

    g_snprintf (sink_name, 80, "sink_%u", i);

    h[i] =
        gst_harness_new_with_element (h[num_inputs]->element, sink_name, NULL);
    fail_if (NULL == h[i], "Unable to create Test TIOVXMux harness");

    gst_harness_set_src_caps_str (h[i],
        "video/x-raw, format=RGBx, width=320, height=240, num-channels=1");
  }
}

GST_START_TEST (test_success)
{
  GstBuffer *out_buf = NULL;
  GstTIOVXMuxMeta *tiovx_meta = NULL;
  vx_size array_size = 0;
  guint i = 0;
  guint num_inputs = 2;
  GstBuffer *in_buf[2] = { NULL, NULL };
  void *in_ptrs[2] = { NULL, NULL };
  GstHarness *h[3] = { NULL, NULL, NULL };

  initialize_harness_and_element (&h[0], num_inputs);

  /* create a buffer of the appropiate size */
  for (i = 0; i < num_inputs; i++) {
    GstTIOVXMemoryData *ti_memory = NULL;
    GstMemory *memory = NULL;

    in_buf[i] =
        gst_harness_create_buffer (h[i], kImageWidth * kImageHeight * 4);

    memory = gst_buffer_get_memory (in_buf[i], 0);

    ti_memory = gst_tiovx_memory_get_data (memory);
    in_ptrs[i] = (void *) ti_memory->mem_ptr.host_ptr;

    /* push the buffer into the queue */
    gst_harness_push (h[i], in_buf[i]);
  }

  /* pull the buffer from the queue */
  out_buf = gst_harness_pull (h[num_inputs]);

  fail_if (out_buf == NULL);

  tiovx_meta =
      (GstTIOVXMuxMeta *) gst_buffer_get_meta (out_buf,
      GST_TYPE_TIOVX_MUX_META_API);
  fail_if (NULL == tiovx_meta);
  fail_if (NULL == tiovx_meta->array);

  vxQueryObjectArray (tiovx_meta->array, VX_OBJECT_ARRAY_NUMITEMS, &array_size,
      sizeof (array_size));

  fail_if (num_inputs != array_size);

  for (i = 0; i < array_size; i++) {
    void *addr[module_max_num_addr] = { NULL };
    vx_uint32 sizes[module_max_num_addr];
    vx_reference output_ref = NULL;
    uint32_t num_addrs = 0;

    output_ref = vxGetObjectArrayItem (tiovx_meta->array, i);

    tivxReferenceExportHandle (output_ref,
        addr, sizes, module_max_num_addr, &num_addrs);

    fail_if (addr[0] != in_ptrs[i]);

    vxReleaseReference (&output_ref);

    /* Check that the input buffers are still valid, and therefore the memory */
    fail_if (GST_MINI_OBJECT_REFCOUNT (in_buf[i]) == 0);
  }

  /* cleanup */
  gst_buffer_unref (out_buf);
  for (i = 0; i < num_inputs + 1; i++) {
    gst_harness_teardown (h[i]);
  }
}

GST_END_TEST;

static Suite *
gst_tiovx_mux_suite (void)
{
  Suite *suite = suite_create ("tiovxmux");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_success);

  return suite;
}

GST_CHECK_MAIN (gst_tiovx_mux);
