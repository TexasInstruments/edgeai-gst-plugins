/*
 * Copyright (C) 2020-2021 RidgeRun, LLC (http://www.ridgerun.com)
 * All Rights Reserved.
 *
 * The contents of this software are proprietary and confidential to RidgeRun,
 * LLC.  No part of this program may be photocopied, reproduced or translated
 * into another programming language without prior written consent of
 * RidgeRun, LLC.  The user is free to modify the source code after obtaining
 * a software license from RidgeRun.  All source code changes must be provided
 * back to RidgeRun without any encumbrance.
*/

#ifndef __TEST_UTILS_H__
#define __TEST_UTILS_H__

#include <gst/check/gstcheck.h>

#define NUMBER_OF_STATE_CHANGES 5

GstElement *test_create_pipeline (const gchar * pipe_desc);
void test_states_change (const gchar * pipe_desc);
void test_fail_properties_configuration (const gchar * pipe_desc);
void test_states_change_success (const gchar * pipe_desc);

void
test_states_change_fail (const gchar * pipe_desc);
static void
test_states_change_base (const gchar * pipe_desc, GstStateChangeReturn *state_change);

GstElement *
test_create_pipeline (const gchar * pipe_desc)
{
  GstElement *pipeline = NULL;
  GError *error = NULL;

  GST_INFO ("testing pipeline %s", pipe_desc);

  pipeline = gst_parse_launch (pipe_desc, &error);

  /* Check for errors creating pipeline */
  fail_if (error != NULL, error);
  fail_if (pipeline == NULL, error);

  return pipeline;
}

static void
test_states_change_base (const gchar * pipe_desc, GstStateChangeReturn *state_change)
{
    GstElement *pipeline = NULL;
    gint i = 0;

    pipeline = test_create_pipeline (pipe_desc);

    for (i = 0; i < NUMBER_OF_STATE_CHANGES; i++) {

      fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
          state_change[0]);
      fail_unless_equals_int (gst_element_get_state (pipeline, NULL, NULL, -1),
          state_change[1]);
      fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
          state_change[2]);
    }
    gst_object_unref (pipeline);
}

void
test_states_change (const gchar * pipe_desc)
{
    GstStateChangeReturn state_change[] = {GST_STATE_CHANGE_ASYNC, GST_STATE_CHANGE_SUCCESS, GST_STATE_CHANGE_SUCCESS};

    test_states_change_base(pipe_desc, state_change);
}

void
test_states_change_fail (const gchar * pipe_desc)
{
    GstStateChangeReturn state_change[] = {GST_STATE_CHANGE_FAILURE, GST_STATE_CHANGE_FAILURE, GST_STATE_CHANGE_FAILURE};

    test_states_change_base(pipe_desc, state_change);
}

void
test_states_change_success (const gchar * pipe_desc)
{
    GstStateChangeReturn state_change[] = {GST_STATE_CHANGE_SUCCESS, GST_STATE_CHANGE_SUCCESS, GST_STATE_CHANGE_SUCCESS};

    test_states_change_base(pipe_desc, state_change);
}

#endif
