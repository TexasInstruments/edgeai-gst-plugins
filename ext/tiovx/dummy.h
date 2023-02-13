#ifndef __GST_DUMMY_H__
#define __GST_DUMMY_H__

#include <gst/base/base-prelude.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_DUMMY (gst_dummy_get_type())
G_DECLARE_FINAL_TYPE (GstDummy, gst_dummy, GST,
    DUMMY, GstElement)

G_END_DECLS
#endif                          /* __GST_DUMMY_H__ */
