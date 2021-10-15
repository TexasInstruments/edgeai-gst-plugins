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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst-libs/gst/tiovx/gsttiovxallocator.h>
#include <gst-libs/gst/tiovx/gsttiovxtensorbufferpool.h>
#include <gst-libs/gst/tiovx/gsttiovxtensormeta.h>
#include <gst-libs/gst/tiovx/gsttiovxbufferpoolutils.h>
#include <gst-libs/gst/tiovx/gsttiovxutils.h>
#include <gst/check/gstcheck.h>

#include <app_init.h>

#define KNUMDIMS 3
#define KDIMSSIZE 100
#define KDATATYPE VX_TYPE_UINT8
#define KSIZE 1000000           /* KDIMSSIZE ^ KNUMDIMS */

static const int kMinBuffers = 1;
static const int kMaxBuffers = 4;

static GstBufferPool *
get_pool (void)
{
  GstTIOVXTensorBufferPool *tiovx_pool = NULL;

  if (0 != appCommonInit ()) {
    goto err_exit;
  }
  tivxInit ();
  tivxHostInit ();

  tiovx_pool = g_object_new (GST_TYPE_TIOVX_TENSOR_BUFFER_POOL, NULL);

  return GST_BUFFER_POOL (tiovx_pool);

err_exit:
  return NULL;
}

static void
test_new_buffer (vx_enum data_type, vx_size expected_size)
{
  GstBufferPool *pool = get_pool ();
  GstStructure *conf = NULL;
  GstCaps *caps = NULL;
  GstBuffer *buf = NULL;
  GstTIOVXTensorMeta *meta = NULL;
  gboolean ret = FALSE;
  vx_tensor tensor = NULL;
  vx_context context = NULL;
  vx_reference reference = NULL;
  vx_size dims[KNUMDIMS];
  vx_size num_dims_query = 0;
  vx_size tensor_sizes[KNUMDIMS];
  vx_enum q_data_type = 0;
  void *dim_addr[MODULE_MAX_NUM_TENSORS] = { NULL };
  vx_uint32 dim_sizes[MODULE_MAX_NUM_TENSORS];
  vx_uint32 num_dims = 0;
  int i = 0;

  fail_if (!GST_TIOVX_IS_TENSOR_BUFFER_POOL (pool));

  conf = gst_buffer_pool_get_config (pool);
  caps = gst_caps_new_simple ("application/x-tensor-tiovx",
      "num-dims", G_TYPE_INT, KNUMDIMS,
      "data-type", G_TYPE_INT, data_type, NULL);

  context = vxCreateContext ();
  fail_if (VX_SUCCESS != vxGetStatus ((vx_reference) context),
      "Failed to create context");

  for (i = 0; i < KNUMDIMS; i++) {
    dims[i] = KDIMSSIZE;
  }

  reference =
      (vx_reference) vxCreateTensor (context, KNUMDIMS, dims, data_type, 0);

  fail_if (VX_SUCCESS != vxGetStatus ((vx_reference) reference),
      "Failed to create tensor reference");

  gst_tiovx_buffer_pool_config_set_exemplar (conf, reference);

  gst_buffer_pool_config_set_params (conf, caps, expected_size, kMinBuffers,
      kMaxBuffers);
  ret = gst_buffer_pool_set_config (pool, conf);
  fail_if (FALSE == ret, "Buffer pool configuration failed");

  gst_buffer_pool_set_active (pool, TRUE);
  gst_buffer_pool_acquire_buffer (pool, &buf, NULL);
  fail_if (NULL == buf, "No buffer has been returned");

  /* Check for a valid vx_tensor */
  meta =
      (GstTIOVXTensorMeta *) gst_buffer_get_meta (buf,
      GST_TYPE_TIOVX_TENSOR_META_API);
  fail_if (NULL == meta, "No Tensor meta in buffer");
  tensor = (vx_tensor) vxGetObjectArrayItem (meta->array, 0);

  vxQueryTensor (tensor, VX_TENSOR_NUMBER_OF_DIMS, &num_dims_query,
      sizeof (vx_size));
  vxQueryTensor (tensor, VX_TENSOR_DIMS, tensor_sizes,
      num_dims_query * sizeof (vx_size));
  vxQueryTensor (tensor, VX_TENSOR_DATA_TYPE, &q_data_type, sizeof (vx_enum));

  fail_if (num_dims_query != KNUMDIMS,
      "Stored vx_tensor has the incorrect num dims. Expected: %ud\t Got: %lu",
      KNUMDIMS, num_dims_query);
  fail_if (meta->tensor_info.num_dims != KNUMDIMS,
      "Meta has the incorrect num dims. Expected: %ud\t Got: %lu", KNUMDIMS,
      num_dims_query);
  fail_if (q_data_type != data_type,
      "Stored vx_tensor has the incorrect data type. Expected: %u\t Got: %u",
      data_type, q_data_type);
  fail_if (meta->tensor_info.data_type != data_type,
      "Meta has the incorrect data type. Expected: %u\t Got: %u", data_type,
      q_data_type);

  for (i = 0; i < KNUMDIMS; i++) {
    fail_if (dims[i] != tensor_sizes[i],
        "Stored vx_tensor has the incorrect sizes for dimension %u. Expected: %ud\t Got: %lu",
        i, dims[i], tensor_sizes[i]);
    fail_if (dims[i] != meta->tensor_info.dim_sizes[i],
        "Meta has the incorrect sizes for dimension %u. Expected: %ud\t Got: %lu",
        i, dims[i], meta->tensor_info.dim_sizes[i]);
  }

  /* Check memory size */
  tivxReferenceExportHandle ((vx_reference) tensor,
      dim_addr, dim_sizes, MODULE_MAX_NUM_TENSORS, &num_dims);

  fail_if (1 != num_dims,
      "Number of dimensions in memory should be always 1. Got: %lu", num_dims);
  fail_if (expected_size != dim_sizes[0],
      "Wrong memory size in buffer. Expected: %lu\t Got: %lu", expected_size,
      dim_sizes[0]);
  fail_if (expected_size != meta->tensor_info.tensor_size,
      "Meta has wrong tensor memory size. Expected: %lu\t Got: %lu",
      expected_size, meta->tensor_info.tensor_size);

  gst_buffer_unref (buf);
  gst_buffer_pool_set_active (pool, FALSE);
  gst_object_unref (pool);
  gst_caps_unref (caps);
  vxReleaseReference (&reference);
  vxReleaseContext (&context);
}

GST_START_TEST (test_new_buffer_uint8)
{
  test_new_buffer (VX_TYPE_UINT8, 1000000);
}

GST_END_TEST;

GST_START_TEST (test_new_buffer_int8)
{
  test_new_buffer (VX_TYPE_INT8, 1000000);
}

GST_END_TEST;

GST_START_TEST (test_new_buffer_uint16)
{
  test_new_buffer (VX_TYPE_UINT16, 2000000);
}

GST_END_TEST;

GST_START_TEST (test_new_buffer_int16)
{
  test_new_buffer (VX_TYPE_INT16, 2000000);
}

GST_END_TEST;

GST_START_TEST (test_new_buffer_uint32)
{
  test_new_buffer (VX_TYPE_UINT32, 4000000);
}

GST_END_TEST;

GST_START_TEST (test_new_buffer_int32)
{
  test_new_buffer (VX_TYPE_INT32, 4000000);
}

GST_END_TEST;

GST_START_TEST (test_new_buffer_float32)
{
  test_new_buffer (VX_TYPE_FLOAT32, 4000000);
}

GST_END_TEST;

GST_START_TEST (test_new_buffer_empty_caps)
{
  GstBufferPool *pool = get_pool ();
  gboolean ret = FALSE;
  vx_context context = NULL;
  vx_reference reference = NULL;
  vx_status status = VX_SUCCESS;
  GstStructure *conf = gst_buffer_pool_get_config (pool);
  GstCaps *caps = NULL;
  vx_size dims[KNUMDIMS];

  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  reference =
      (vx_reference) vxCreateTensor (context, KNUMDIMS, dims, KDATATYPE, 0);

  gst_tiovx_buffer_pool_config_set_exemplar (conf, reference);
  gst_buffer_pool_config_set_params (conf, caps, KSIZE, kMinBuffers,
      kMaxBuffers);
  ret = gst_buffer_pool_set_config (pool, conf);
  fail_if (ret == TRUE, "Config didn't ignore empty caps");

  vxReleaseReference (&reference);
  vxReleaseContext (&context);
}

GST_END_TEST;

GST_START_TEST (test_new_buffer_invalid_caps)
{
  GstBufferPool *pool = get_pool ();
  gboolean ret = FALSE;
  vx_context context = NULL;
  vx_reference reference = NULL;
  vx_status status = VX_SUCCESS;
  vx_size dims[KNUMDIMS];
  GstStructure *conf = gst_buffer_pool_get_config (pool);
  GstCaps *caps = gst_caps_new_simple ("application/x-tensor",
      "num-dims", G_TYPE_INT, KNUMDIMS,
      "data-type", G_TYPE_INT, KDATATYPE,
      NULL);

  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  reference =
      (vx_reference) vxCreateTensor (context, KNUMDIMS, dims, KDATATYPE, 0);

  gst_tiovx_buffer_pool_config_set_exemplar (conf, reference);
  gst_buffer_pool_config_set_params (conf, caps, KSIZE, kMinBuffers,
      kMaxBuffers);
  ret = gst_buffer_pool_set_config (pool, conf);
  fail_if (ret == TRUE, "Config didn't ignore invalid caps");

  gst_caps_unref (caps);
  vxReleaseReference (&reference);
  vxReleaseContext (&context);
}

GST_END_TEST;

GST_START_TEST (test_new_buffer_invalid_caps_no_num_dims)
{
  GstBufferPool *pool = get_pool ();
  gboolean ret = FALSE;
  vx_context context = NULL;
  vx_reference reference = NULL;
  vx_status status = VX_SUCCESS;
  vx_size dims[KNUMDIMS];
  GstStructure *conf = gst_buffer_pool_get_config (pool);
  GstCaps *caps = gst_caps_new_simple ("application/x-tensor-tiovx",
      "data-type", G_TYPE_INT, KDATATYPE,
      NULL);

  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  reference =
      (vx_reference) vxCreateTensor (context, KNUMDIMS, dims, KDATATYPE, 0);

  gst_tiovx_buffer_pool_config_set_exemplar (conf, reference);
  gst_buffer_pool_config_set_params (conf, caps, KSIZE, kMinBuffers,
      kMaxBuffers);
  ret = gst_buffer_pool_set_config (pool, conf);
  fail_if (ret == TRUE, "Config didn't ignore invalid caps");

  gst_caps_unref (caps);
  vxReleaseReference (&reference);
  vxReleaseContext (&context);
}

GST_END_TEST;

GST_START_TEST (test_new_buffer_invalid_caps_no_data_type)
{
  GstBufferPool *pool = get_pool ();
  gboolean ret = FALSE;
  vx_context context = NULL;
  vx_reference reference = NULL;
  vx_status status = VX_SUCCESS;
  vx_size dims[KNUMDIMS];
  GstStructure *conf = gst_buffer_pool_get_config (pool);
  GstCaps *caps = gst_caps_new_simple ("application/x-tensor-tiovx",
      "num-dims", G_TYPE_INT, KNUMDIMS,
      NULL);

  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  reference =
      (vx_reference) vxCreateTensor (context, KNUMDIMS, dims, KDATATYPE, 0);

  gst_tiovx_buffer_pool_config_set_exemplar (conf, reference);
  gst_buffer_pool_config_set_params (conf, caps, KSIZE, kMinBuffers,
      kMaxBuffers);
  ret = gst_buffer_pool_set_config (pool, conf);
  fail_if (ret == TRUE, "Config didn't ignore invalid caps");

  vxReleaseReference (&reference);
  gst_caps_unref (caps);
  vxReleaseContext (&context);
}

GST_END_TEST;

GST_START_TEST (test_new_buffer_no_set_params)
{
  GstBufferPool *pool = get_pool ();
  gboolean ret = FALSE;
  vx_context context = NULL;
  vx_reference reference = NULL;
  vx_status status = VX_SUCCESS;
  vx_size dims[KNUMDIMS];
  GstStructure *conf = gst_buffer_pool_get_config (pool);

  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  reference =
      (vx_reference) vxCreateTensor (context, KNUMDIMS, dims, KDATATYPE, 0);

  gst_tiovx_buffer_pool_config_set_exemplar (conf, reference);
  ret = gst_buffer_pool_set_config (pool, conf);
  fail_if (ret == TRUE, "Set config didn't ignore empty params");

  vxReleaseReference (&reference);
  vxReleaseContext (&context);
}

GST_END_TEST;

GST_START_TEST (test_new_buffer_empty_exemplar)
{
  GstBufferPool *pool = get_pool ();
  gboolean ret = FALSE;

  GstStructure *conf = gst_buffer_pool_get_config (pool);

  ret = gst_buffer_pool_set_config (pool, conf);
  fail_if (ret == TRUE, "Set config didn't ignore empty exemplar");
}

GST_END_TEST;

GST_START_TEST (test_external_allocator)
{
  GstBufferPool *pool = get_pool ();
  GstAllocator *allocator = g_object_new (GST_TYPE_TIOVX_ALLOCATOR, NULL);
  GstBuffer *buf = NULL;
  GstTIOVXTensorMeta *meta = NULL;
  gboolean ret = FALSE;
  vx_status status = VX_SUCCESS;
  vx_tensor tensor = NULL;
  vx_context context = NULL;
  vx_reference reference = NULL;
  vx_size dims[KNUMDIMS];
  vx_size num_dims_query;
  vx_size tensor_sizes[KNUMDIMS];
  vx_enum data_type;
  void *dim_addr[MODULE_MAX_NUM_TENSORS] = { NULL };
  vx_uint32 dim_sizes[MODULE_MAX_NUM_TENSORS];
  vx_uint32 num_dims = 0;
  int i = 0;
  GstStructure *conf = gst_buffer_pool_get_config (pool);
  GstCaps *caps = gst_caps_new_simple ("application/x-tensor-tiovx",
      "num-dims", G_TYPE_INT, KNUMDIMS,
      "data-type", G_TYPE_INT, KDATATYPE,
      NULL);

  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  for (i = 0; i < KNUMDIMS; i++) {
    dims[i] = KDIMSSIZE;
  }

  reference =
      (vx_reference) vxCreateTensor (context, KNUMDIMS, dims, KDATATYPE, 0);

  gst_tiovx_buffer_pool_config_set_exemplar (conf, reference);
  gst_buffer_pool_config_set_params (conf, caps, KSIZE, kMinBuffers,
      kMaxBuffers);
  gst_buffer_pool_config_set_allocator (conf, allocator, NULL);
  ret = gst_buffer_pool_set_config (pool, conf);

  fail_if (FALSE == ret,
      "Bufferpool configuration failed with external allocator");

  gst_object_unref (allocator);

  gst_buffer_pool_set_active (pool, TRUE);
  gst_buffer_pool_acquire_buffer (pool, &buf, NULL);
  fail_if (NULL == buf, "No buffer has been returned");

  /* Check for a valid vx_tensor */
  meta =
      (GstTIOVXTensorMeta *) gst_buffer_get_meta (buf,
      GST_TYPE_TIOVX_TENSOR_META_API);
  fail_if (NULL == meta, "No Tensor meta in buffer");
  tensor = (vx_tensor) vxGetObjectArrayItem (meta->array, 0);

  vxQueryTensor (tensor, VX_TENSOR_NUMBER_OF_DIMS, &num_dims_query,
      sizeof (vx_size));
  vxQueryTensor (tensor, VX_TENSOR_DIMS, tensor_sizes,
      num_dims_query * sizeof (vx_size));
  vxQueryTensor (tensor, VX_TENSOR_DATA_TYPE, &data_type, sizeof (vx_enum));

  fail_if (num_dims_query != KNUMDIMS,
      "Stored vx_tensor has the incorrect num dims. Expected: %ud\t Got: %lu",
      KNUMDIMS, num_dims_query);
  fail_if (meta->tensor_info.num_dims != KNUMDIMS,
      "Meta has the incorrect num dims. Expected: %ud\t Got: %lu", KNUMDIMS,
      num_dims_query);
  fail_if (data_type != KDATATYPE,
      "Stored vx_tensor has the incorrect data type. Expected: %u\t Got: %u",
      KDATATYPE, data_type);
  fail_if (meta->tensor_info.data_type != KDATATYPE,
      "Meta has the incorrect data type. Expected: %u\t Got: %u", KDATATYPE,
      data_type);

  for (i = 0; i < KNUMDIMS; i++) {
    fail_if (dims[i] != tensor_sizes[i],
        "Stored vx_tensor has the incorrect sizes for dimension %u. Expected: %ud\t Got: %lu",
        i, dims[i], tensor_sizes[i]);
    fail_if (dims[i] != meta->tensor_info.dim_sizes[i],
        "Meta has the incorrect sizes for dimension %u. Expected: %ud\t Got: %lu",
        i, dims[i], meta->tensor_info.dim_sizes[i]);
  }

  /* Check memory size */
  tivxReferenceExportHandle ((vx_reference) tensor,
      dim_addr, dim_sizes, MODULE_MAX_NUM_TENSORS, &num_dims);

  fail_if (1 != num_dims,
      "Number of dimensions in memory should be always 1. Got: %lu", num_dims);
  fail_if (KSIZE != dim_sizes[0],
      "Wrong memory size in buffer. Expected: %lu\t Got: %lu", KSIZE,
      dim_sizes[0]);
  fail_if (KSIZE != meta->tensor_info.tensor_size,
      "Meta has wrong tensor memory size. Expected: %lu\t Got: %lu", KSIZE,
      meta->tensor_info.tensor_size);

  gst_buffer_unref (buf);
  gst_buffer_pool_set_active (pool, FALSE);
  gst_object_unref (pool);
  gst_caps_unref (caps);
  vxReleaseReference (&reference);
  vxReleaseContext (&context);
}

GST_END_TEST;

static Suite *
gst_tiovx_tensor_buffer_pool_suite (void)
{
  Suite *s = suite_create ("GstTIOVXTensorBufferPool");
  TCase *tc_chain = tcase_create ("tiovx_tensor_buffer_pool tests");

  tcase_set_timeout (tc_chain, 0);

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_new_buffer_uint8);
  tcase_add_test (tc_chain, test_new_buffer_int8);
  tcase_add_test (tc_chain, test_new_buffer_uint16);
  tcase_add_test (tc_chain, test_new_buffer_int16);
  tcase_add_test (tc_chain, test_new_buffer_uint32);
  tcase_add_test (tc_chain, test_new_buffer_int32);
  tcase_add_test (tc_chain, test_new_buffer_float32);
  tcase_add_test (tc_chain, test_new_buffer_empty_caps);
  tcase_add_test (tc_chain, test_new_buffer_invalid_caps);
  tcase_add_test (tc_chain, test_new_buffer_invalid_caps_no_num_dims);
  tcase_add_test (tc_chain, test_new_buffer_invalid_caps_no_data_type);
  tcase_add_test (tc_chain, test_new_buffer_no_set_params);
  tcase_add_test (tc_chain, test_new_buffer_empty_exemplar);
  tcase_add_test (tc_chain, test_external_allocator);

  return s;
}

GST_CHECK_MAIN (gst_tiovx_tensor_buffer_pool);
