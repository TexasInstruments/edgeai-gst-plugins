extern "C"
{

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "dummy.h"

}

#define GST_DUMMY_GET_CLASS(obj)  \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_DUMMY, \
                              GstDummyClass))


/* Pads definitions */
static
    GstStaticPadTemplate
    src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
    );

static
    GstStaticPadTemplate
    sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
    );

struct _GstDummy
{
  GstElement
      element;
  GstPad *src_pad;
  GstPad *sink_pad;
};

GST_DEBUG_CATEGORY_STATIC (gst_dummy_debug);
#define GST_CAT_DEFAULT gst_dummy_debug

static void
gst_dummy_child_proxy_init (gpointer g_iface, gpointer iface_data);
static gboolean gst_dummy_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_dummy_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query);

#define gst_dummy_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstDummy, gst_dummy,
    GST_TYPE_ELEMENT,
    G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY,
        gst_dummy_child_proxy_init););

static void
gst_dummy_finalize (GObject * obj);

static
    GstFlowReturn
gst_dummy_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);

static
    gboolean
gst_dummy_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

/* Initialize the plugin's class */
static void
gst_dummy_class_init (GstDummyClass * klass)
{
  GObjectClass *
      gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *
      gstelement_class = GST_ELEMENT_CLASS (klass);
  GstPadTemplate *
      pad_template = NULL;

  gst_element_class_set_details_simple (gstelement_class,
      "TI DL PostProc",
      "Filter/Converter/Video",
      "Dummy",
      "Rahul T R <r-ravikumar@ti.com>");

  pad_template =
      gst_pad_template_new_from_static_pad_template_with_gtype (&src_template,
      GST_TYPE_PAD);
  gst_element_class_add_pad_template (gstelement_class, pad_template);

  pad_template =
      gst_pad_template_new_from_static_pad_template_with_gtype
      (&sink_template, GST_TYPE_PAD);
  gst_element_class_add_pad_template (gstelement_class, pad_template);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_dummy_finalize);

  GST_DEBUG_CATEGORY_INIT (gst_dummy_debug,
      "dummy", 0, "dummy");
}

/* Initialize the new element */
static void
gst_dummy_init (GstDummy * self)
{
  GstDummyClass *
      klass = GST_DUMMY_GET_CLASS (self);
  GstElementClass *
      gstelement_class = GST_ELEMENT_CLASS (klass);
  GstPadTemplate *
      pad_template = NULL;

  GST_LOG_OBJECT (self, "init");

  pad_template = gst_element_class_get_pad_template (gstelement_class, "sink");
  self->sink_pad = gst_pad_new_from_template (pad_template, "sink");

  pad_template = gst_element_class_get_pad_template (gstelement_class, "src");
  self->src_pad = gst_pad_new_from_template (pad_template, "src");

  gst_pad_set_event_function (self->sink_pad,
      GST_DEBUG_FUNCPTR (gst_dummy_sink_event));

  gst_pad_set_chain_function (self->sink_pad,
      GST_DEBUG_FUNCPTR (gst_dummy_chain));

  gst_pad_set_query_function (self->sink_pad,
      GST_DEBUG_FUNCPTR (gst_dummy_sink_query));

  gst_pad_set_query_function (self->src_pad,
      GST_DEBUG_FUNCPTR (gst_dummy_src_query));

  gst_element_add_pad (GST_ELEMENT (self), self->src_pad);
  gst_pad_set_active (self->src_pad, TRUE);
  gst_element_add_pad (GST_ELEMENT (self), self->sink_pad);
  gst_pad_set_active (self->sink_pad, TRUE);
}

static void
gst_dummy_finalize (GObject * obj)
{
  GstDummy *
      self = GST_DUMMY (obj);

  GST_LOG_OBJECT (self, "finalize");
  G_OBJECT_CLASS (gst_dummy_parent_class)->finalize (obj);
}

static
    gboolean
gst_dummy_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstDummy *
      self = GST_DUMMY (parent);
  gboolean ret = FALSE;

  GST_LOG_OBJECT (self, "sink_event");

  gst_pad_push_event (self->src_pad, event);
  ret = gst_pad_event_default (pad, parent, event);

  return ret;
}

static
    GstFlowReturn
gst_dummy_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstDummy *self = GST_DUMMY (parent);

  GST_LOG_OBJECT (self, "chain");

  ret = gst_pad_push (self->src_pad, buffer);

  GST_LOG_OBJECT (self, "chain ret %d", ret);

  return ret;
}

/* GstChildProxy implementation */
static GObject *
gst_dummy_child_proxy_get_child_by_index (GstChildProxy *
    child_proxy, guint index)
{
  return NULL;
}

static GObject *
gst_dummy_child_proxy_get_child_by_name (GstChildProxy *
    child_proxy, const gchar * name)
{
  GstDummy *
      self = GST_DUMMY (child_proxy);
  GObject *
      obj = NULL;

  GST_OBJECT_LOCK (self);

  if (0 == strcmp (name, "src")) {
    obj = G_OBJECT (self->src_pad);
  } else if (0 == strcmp (name, "sink")) {
    obj = G_OBJECT (self->sink_pad);
  }

  if (obj) {
    gst_object_ref (obj);
  }

  GST_OBJECT_UNLOCK (self);

  return obj;
}

static
    guint
gst_dummy_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  /* Number of pads is always 3 (image pad, tensor pad and src pad) */
  return 3;
}

static void
gst_dummy_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *
      iface = (GstChildProxyInterface *) g_iface;

  iface->get_child_by_index =
      gst_dummy_child_proxy_get_child_by_index;
  iface->get_child_by_name = gst_dummy_child_proxy_get_child_by_name;
  iface->get_children_count =
      gst_dummy_child_proxy_get_children_count;
}

static gboolean
gst_dummy_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstDummy *
      self = GST_DUMMY (parent);
  gboolean ret = FALSE;

  ret = gst_pad_peer_query(self->sink_pad, query);
  return ret;
}

static gboolean
gst_dummy_sink_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstDummy *
      self = GST_DUMMY (parent);
  gboolean ret = FALSE;

  ret = gst_pad_peer_query(self->src_pad, query);
  return ret;
}
