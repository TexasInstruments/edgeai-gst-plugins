/*
* Copyright (C) <2021> RidgeRun, LLC (http://www.ridgerun.com)
* All Rights Reserved.
*
* The contents of this software are proprietary and confidential to RidgeRun,
* LLC.  No part of this program may be photocopied, reproduced or translated
* into another programming language without prior written consent of
* RidgeRun, LLC.  The user is free to modify the source code after obtaining
* a software license from RidgeRun.  All source code changes must be provided
* back to RidgeRun without any encumbrance.
*/

#include <gst/check/gstcheck.h>

static const gchar *test_lines[] = {
  "videotestsrc ! fakesink async=false ",       /* start pipeline */
  NULL,
};

enum
{
  /* test names */
  TEST_BUILD_PIPELINE,
};


GST_START_TEST (test_success)
{
  GstElement *pipeline;
  GstStateChangeReturn ret;
  GError *error = NULL;
  int i;

  pipeline = gst_parse_launch (test_lines[TEST_BUILD_PIPELINE], &error);

  fail_if (error != NULL, error);
  fail_if (pipeline == NULL, error);

  for (i = 0; i < 10; i++) {
    ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
    fail_if (GST_STATE_CHANGE_FAILURE == ret);
    fail_if (GST_STATE (pipeline) != GST_STATE_PLAYING);
    ret = gst_element_set_state (pipeline, GST_STATE_NULL);
    fail_if (GST_STATE_CHANGE_FAILURE == ret);
  }
  gst_object_unref (pipeline);
}

GST_END_TEST;


static Suite *
gst_state_suite (void)
{
  Suite *suite = suite_create ("gst_state");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_success);

  return suite;
}

GST_CHECK_MAIN (gst_state);
