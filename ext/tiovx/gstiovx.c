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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>

#include "gsttransform.h"

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
tiovx_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "transform", GST_RANK_NONE,
      GST_TYPE_PLUGIN_TEMPLATE);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, tiovx,
    "GStreamer plugin for TIOVX", tiovx_init, PACKAGE_VERSION, "Proprietary",
    GST_PACKAGE_NAME, "http://ridgerun.com/")
