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

#ifndef __GST_PLUGIN_TEMPLATE_H__
#define __GST_PLUGIN_TEMPLATE_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_PLUGIN_TEMPLATE (gst_plugin_template_get_type())
G_DECLARE_FINAL_TYPE (GstPluginTemplate, gst_plugin_template,
    GST, PLUGIN_TEMPLATE, GstBaseTransform)

struct _GstPluginTemplate {
  GstBaseTransform element;

  gboolean silent;
};

G_END_DECLS

#endif /* __GST_PLUGIN_TEMPLATE_H__ */
