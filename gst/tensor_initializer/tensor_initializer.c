/**
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2018 Jijoong Moon <jijoong.moon@samsung.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 */

/**
 * @file	tensor_initializer.c
 * @date	26 June 2018
 * @brief	test element to check tensors
 * @see		https://github.com/nnsuite/nnstreamer
 * @author	Jijoong Moon <jijoong.moon@samsung.com>
 * @bug		No known bugs except for NYI items
 */

/**
 * SECTION:element-tensor_initializer
 *
 * This is the element to test tensors only.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! tensor_initializer ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include "tensor_initializer.h"

GST_DEBUG_CATEGORY_STATIC (gst_tensor_initializer_debug);
#define GST_CAT_DEFAULT gst_tensor_initializer_debug

/** Properties */
enum
{
  PROP_0,
  PROP_PASSTHROUGH,
  PROP_SILENT
};

/**
 * the capabilities of the inputs and outputs.
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (GST_TENSOR_CAP_DEFAULT)
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_TENSOR_CAP_DEFAULT)
    );

static inline gboolean
gst_tensor_initializer_is_active_sinkpad (GstTensorInitializer * initializer,
    GstPad * pad);
static GstPad *gst_tensor_initializer_get_active_sinkpad (GstTensorInitializer *
    initializer);
static GstPad *gst_tensor_initializer_get_linked_pad (GstTensorInitializer *
    initializer, GstPad * pad, gboolean strict);
static gboolean gst_tensor_initializer_set_active_pad (GstTensorInitializer *
    self, GstPad * pad);

#define GST_TYPE_INITIALIZER_PAD \
  (gst_initializer_pad_get_type())
#define GST_INITIALIZER_PAD(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_INITIALIZER_PAD, GstInitializerPad))
#define GST_INITIALIZER_PAD_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_INITIALIZER_PAD, GstInitializerPadClass))
#define GST_IS_INITIALIZER_PAD(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_INITIALIZER_PAD))
#define GST_IS_INITIALIZER_PAD_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_INITIALIZER_PAD))
#define GST_INITIALIZER_PAD_CAST(obj) \
  ((GstInitializerPad *)(obj))

typedef struct _GstInitializerPad GstInitializerPad;
typedef struct _GstInitializerPadClass GstInitializerPadClass;
typedef struct _GstInitializerPadCachedBuffer GstInitializerPadCachedBuffer;

struct _GstInitializerPad
{
  GstPad parent;

  gboolean pushed;              /* when buffer was pushed downstream since activation */
  guint group_id;               /* Group ID from the last stream-start */
  gboolean group_done;          /* when Stream Group Done has been
                                   received */
  gboolean eos;                 /* when EOS has been received */
  gboolean eos_sent;            /* when EOS was sent downstream */
  gboolean discont;             /* after switching we create a discont */
  gboolean flushing;            /* set after flush-start and before flush-stop */
  gboolean always_ok;
  GstTagList *tags;             /* last tags received on the pad */

  GstSegment segment;           /* the current segment on the pad */
  guint32 segment_seqnum;       /* sequence number of the current segment */

  gboolean events_pending;      /* TRUE if sticky events need to be updated */

  gboolean sending_cached_buffers;
  GQueue *cached_buffers;
};

struct _GstInitializerPadCachedBuffer
{
  GstBuffer *buffer;
  GstSegment segment;
};

struct _GstInitializerPadClass
{
  GstPadClass parent;
};


GType gst_initializer_pad_get_type (void);
static void gst_initializer_pad_finalize (GObject * object);
static void gst_initializer_pad_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_initializer_pad_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);

/* static gint64 gst_initializer_pad_get_running_time (GstInitializerPad * pad); */
static void gst_initializer_pad_reset (GstInitializerPad * pad);
static gboolean gst_initializer_pad_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_initializer_pad_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static GstIterator *gst_initializer_pad_iterate_linked_pads (GstPad * pad,
    GstObject * parent);
static GstFlowReturn gst_initializer_pad_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buf);

G_DEFINE_TYPE (GstInitializerPad, gst_initializer_pad, GST_TYPE_PAD);


static void
gst_initializer_pad_class_init (GstInitializerPadClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_initializer_pad_finalize;

  gobject_class->get_property = gst_initializer_pad_get_property;
  gobject_class->set_property = gst_initializer_pad_set_property;
}

static void
gst_initializer_pad_init (GstInitializerPad * pad)
{
  gst_initializer_pad_reset (pad);
}

static void
gst_initializer_pad_finalize (GObject * object)
{
  GstInitializerPad *pad;
  pad = GST_INITIALIZER_PAD_CAST (object);

  if (pad->tags)
    gst_tag_list_unref (pad->tags);

  G_OBJECT_CLASS (gst_initializer_pad_parent_class)->finalize (object);
}

static void
gst_initializer_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  /* GstInitializerPad *spad = GST_INITIALIZER_PAD_CAST (object); */

}

static void
gst_initializer_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  /* GstInitializerPad *spad = GST_INITIALIZER_PAD_CAST (object); */
}

/* static gint64 */
/* gst_initializer_pad_get_running_time (GstInitializerPad * pad) */
/* { */
/*   gint64 ret = 0; */

/*   GST_OBJECT_LOCK (pad); */
/*   if (pad->segment.format == GST_FORMAT_TIME) { */
/*     ret = */
/*         gst_segment_to_running_time (&pad->segment, pad->segment.format, */
/*         pad->segment.position); */
/*   } */
/*   GST_OBJECT_UNLOCK (pad); */

/*   GST_DEBUG_OBJECT (pad, "running time: %" GST_TIME_FORMAT */
/*       " segment: %" GST_SEGMENT_FORMAT, GST_TIME_ARGS (ret), &pad->segment); */

/*   return ret; */
/* } */

static void
gst_initializer_pad_reset (GstInitializerPad * pad)
{
  GST_OBJECT_LOCK (pad);
  pad->pushed = FALSE;
  pad->group_done = FALSE;
  pad->eos = FALSE;
  pad->eos_sent = FALSE;
  pad->events_pending = FALSE;
  pad->discont = FALSE;
  pad->flushing = FALSE;
  gst_segment_init (&pad->segment, GST_FORMAT_UNDEFINED);
  GST_OBJECT_UNLOCK (pad);
}


static GstIterator *
gst_initializer_pad_iterate_linked_pads (GstPad * pad, GstObject * parent)
{
  GstTensorInitializer *initializer;
  GstPad *otherpad;
  GstIterator *it = NULL;
  GValue val = { 0, };

  initializer = GST_TENSOR_INITIALIZER (parent);

  otherpad = gst_tensor_initializer_get_linked_pad (initializer, pad, TRUE);
  if (otherpad) {
    g_value_init (&val, GST_TYPE_PAD);
    g_value_set_object (&val, otherpad);
    it = gst_iterator_new_single (GST_TYPE_PAD, &val);
    g_value_unset (&val);
    gst_object_unref (otherpad);
  }

  return it;
}

static gboolean
forward_sticky_events (GstPad * sinkpad, GstEvent ** event, gpointer user_data)
{
  GstTensorInitializer *initializer = GST_TENSOR_INITIALIZER (user_data);
  GST_DEBUG_OBJECT (sinkpad, "forward sticky event %" GST_PTR_FORMAT, *event);

  if (GST_EVENT_TYPE (*event) == GST_EVENT_SEGMENT) {
    GstSegment *seg = &GST_INITIALIZER_PAD (sinkpad)->segment;
    GstEvent *e;

    e = gst_event_new_segment (seg);
    gst_event_set_seqnum (e,
        GST_INITIALIZER_PAD_CAST (sinkpad)->segment_seqnum);

    gst_pad_push_event (initializer->srcpad, e);
  } else if (GST_EVENT_TYPE (*event) == GST_EVENT_STREAM_START
      && !initializer->have_group_id) {
    GstEvent *tmp =
        gst_pad_get_sticky_event (initializer->srcpad, GST_EVENT_STREAM_START,
        0);

    /* Only push stream-start once if not all our streams have a stream-id */
    if (!tmp) {
      gst_pad_push_event (initializer->srcpad, gst_event_ref (*event));
    } else {
      gst_event_unref (tmp);
    }
  } else {
    gst_pad_push_event (initializer->srcpad, gst_event_ref (*event));
  }

  return TRUE;
}


static gboolean
gst_tensor_initializer_eos_wait (GstTensorInitializer * self,
    GstInitializerPad * pad, GstEvent * eos_event)
{
  while (!self->eos && !self->flushing && !pad->flushing) {
    GstPad *active_sinkpad;
    active_sinkpad = gst_tensor_initializer_get_active_sinkpad (self);
    printf ("active_sinkpad @eos_wait : %s vs. pad name : %s \n",
        gst_pad_get_name (active_sinkpad), gst_pad_get_name (pad));

    if (pad == GST_INITIALIZER_PAD_CAST (active_sinkpad) && pad->eos
        && !pad->eos_sent) {
      GST_DEBUG_OBJECT (pad, "send EOS event");
      GST_TENSOR_INITIALIZER_UNLOCK (self);
      /* if we have a pending events, push them now */
      if (pad->events_pending) {
        gst_pad_sticky_events_foreach (GST_PAD_CAST (pad),
            forward_sticky_events, self);
        pad->events_pending = FALSE;
      }

      gst_pad_push_event (self->srcpad, gst_event_ref (eos_event));
      GST_TENSOR_INITIALIZER_LOCK (self);
      /* Wake up other pads so they can continue when syncing to
       * running time, as this pad just switched to EOS and
       * may enable others to progress */
      GST_TENSOR_INITIALIZER_BROADCAST (self);
      pad->eos_sent = TRUE;
    } else {
      /* we can be unlocked here when we are shutting down (flushing) or when we
       * get unblocked */
      GST_TENSOR_INITIALIZER_WAIT (self);
    }
  }

  return self->flushing;
}

static gboolean
gst_tensor_initializer_all_eos (GstTensorInitializer * initializer)
{
  GList *walk;

  for (walk = GST_ELEMENT_CAST (initializer)->sinkpads; walk; walk = walk->next) {
    GstInitializerPad *inipad;

    inipad = GST_INITIALIZER_PAD_CAST (walk->data);
    printf ("name pad @ all_eos : %s\n", gst_pad_get_name (inipad));

    if (!inipad->eos) {
      return FALSE;
    }

  }

  return TRUE;
}

static gboolean
gst_initializer_pad_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res = TRUE;
  gboolean forward;
  gboolean new_tags = FALSE;
  GstTensorInitializer *initializer;
  GstInitializerPad *inipad;
  GstPad *prev_active_sinkpad;
  GstPad *active_sinkpad;

  initializer = GST_TENSOR_INITIALIZER (parent);
  inipad = GST_INITIALIZER_PAD_CAST (pad);

  GST_TENSOR_INITIALIZER_LOCK (initializer);
  prev_active_sinkpad =
      initializer->active_sinkpad ? gst_object_ref (initializer->
      active_sinkpad) : NULL;
  active_sinkpad = gst_tensor_initializer_get_active_sinkpad (initializer);
  gst_object_ref (active_sinkpad);
  GST_TENSOR_INITIALIZER_UNLOCK (initializer);

  printf ("Got Event %s, %s, %s\n", gst_event_type_get_name (event->type),
      gst_pad_get_name (prev_active_sinkpad),
      gst_pad_get_name (active_sinkpad));


  if (prev_active_sinkpad != active_sinkpad) {
    if (prev_active_sinkpad)
      g_object_notify (G_OBJECT (prev_active_sinkpad), "active");
    g_object_notify (G_OBJECT (active_sinkpad), "active");
    g_object_notify (G_OBJECT (initializer), "active-pad");
  }

  if (prev_active_sinkpad)
    gst_object_unref (prev_active_sinkpad);
  gst_object_unref (active_sinkpad);

  GST_TENSOR_INITIALIZER_LOCK (initializer);

  /* forward = (pad == active_sinkpad); */
  /* active_sinkpad = prev_active_sinkpad; */

  forward = (pad == active_sinkpad);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_STREAM_START:{
      if (!gst_event_parse_group_id (event, &inipad->group_id)) {
        initializer->have_group_id = FALSE;
        inipad->group_id = 0;
      }
      break;
    }
    case GST_EVENT_FLUSH_START:
      inipad->flushing = TRUE;
      initializer->eos = FALSE;
      inipad->group_done = FALSE;
      GST_TENSOR_INITIALIZER_BROADCAST (initializer);
      break;
    case GST_EVENT_SEGMENT:
    {
      gst_event_copy_segment (event, &inipad->segment);
      inipad->segment_seqnum = gst_event_get_seqnum (event);
      break;
    }
    case GST_EVENT_TAG:
    {
      GstTagList *tags, *oldtags, *newtags;

      gst_event_parse_tag (event, &tags);
      GST_OBJECT_LOCK (inipad);
      oldtags = inipad->tags;

      newtags = gst_tag_list_merge (oldtags, tags, GST_TAG_MERGE_REPLACE);
      inipad->tags = newtags;
      GST_OBJECT_UNLOCK (inipad);

      if (oldtags)
        gst_tag_list_unref (oldtags);

      new_tags = TRUE;
      break;
    }
    case GST_EVENT_EOS:
      inipad->eos = TRUE;
      GST_DEBUG_OBJECT (pad, "received EOS");
      if (gst_tensor_initializer_all_eos (initializer)) {
        initializer->eos = TRUE;
        GST_TENSOR_INITIALIZER_BROADCAST (initializer);
      } else {
        gst_tensor_initializer_eos_wait (initializer, inipad, event);
        forward = FALSE;
      }
      break;
    case GST_EVENT_STREAM_GROUP_DONE:{
      GST_DEBUG_OBJECT (initializer,
          "Stream group-done in inputselector pad %s",
          GST_OBJECT_NAME (inipad));
      gst_event_parse_stream_group_done (event, &inipad->group_id);
      inipad->group_done = TRUE;
      if (active_sinkpad == pad)
        GST_TENSOR_INITIALIZER_BROADCAST (initializer);
      break;
    }
    default:
      break;
  }

  GST_TENSOR_INITIALIZER_UNLOCK (initializer);

  if (new_tags)
    g_object_notify (G_OBJECT (inipad), "tags");
  if (forward) {
    GST_DEBUG_OBJECT (pad, "forwarding event");
    res = gst_pad_push_event (initializer->srcpad, event);
  } else {
    if (GST_EVENT_IS_STICKY (event))
      inipad->events_pending = TRUE;
    gst_event_unref (event);
  }
  return res;
}


static gboolean
gst_initializer_pad_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean res = FALSE;
  GstTensorInitializer *self = (GstTensorInitializer *) parent;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    case GST_QUERY_POSITION:
    case GST_QUERY_DURATION:
      res = gst_pad_peer_query (self->srcpad, query);
      break;
    case GST_QUERY_ALLOCATION:{
      GstPad *active_sinkpad;
      GstTensorInitializer *initializer = GST_TENSOR_INITIALIZER (parent);

      if (GST_PAD_DIRECTION (pad) == GST_PAD_SINK) {
        GST_TENSOR_INITIALIZER_LOCK (initializer);
        active_sinkpad = gst_tensor_initializer_get_active_sinkpad (self);
        GST_TENSOR_INITIALIZER_UNLOCK (initializer);

        if (pad != active_sinkpad) {
          res = FALSE;
          goto done;
        }
      }
    }
    default:
      res = gst_pad_query_default (pad, parent, query);
      break;
  }

done:
  return res;
}


static GstFlowReturn
gst_initializer_pad_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstTensorInitializer *initializer;
  GstFlowReturn res;
  GstPad *active_sinkpad;
  GstPad *prev_active_sinkpad = NULL;
  GstInitializerPad *inipad;

  initializer = GST_TENSOR_INITIALIZER (parent);
  inipad = GST_INITIALIZER_PAD_CAST (pad);

  GST_TENSOR_INITIALIZER_LOCK (initializer);

  if (initializer->flushing) {
    GST_TENSOR_INITIALIZER_UNLOCK (initializer);
    goto flushing;
  }

  prev_active_sinkpad =
      initializer->active_sinkpad ? gst_object_ref (initializer->
      active_sinkpad) : NULL;
  if (GST_BUFFER_PTS (buf) == 0) {
    active_sinkpad =
        gst_element_get_static_pad ((GstElement *) initializer, "sink_0");
  } else {
    active_sinkpad =
        gst_element_get_static_pad ((GstElement *) initializer, "sink_1");
    gst_tensor_initializer_set_active_pad (initializer, active_sinkpad);
  }
  initializer->active_sinkpad = active_sinkpad;

  if (GST_BUFFER_PTS_IS_VALID (buf)) {
    GstClockTime start_time = GST_BUFFER_PTS (buf);

    GST_LOG_OBJECT (pad, "received start time %" GST_TIME_FORMAT,
        GST_TIME_ARGS (start_time));
    if (GST_BUFFER_DURATION_IS_VALID (buf))
      GST_LOG_OBJECT (pad, "received end time %" GST_TIME_FORMAT,
          GST_TIME_ARGS (start_time + GST_BUFFER_DURATION (buf)));

    GST_OBJECT_LOCK (pad);
    inipad->segment.position = start_time;
    GST_OBJECT_UNLOCK (pad);
  }
  printf ("@ chain -- pad : %s vs. active_sinkpad : %s \n",
      gst_pad_get_name (pad), gst_pad_get_name (active_sinkpad));
  if (pad != active_sinkpad)
    goto ignore;

  GST_TENSOR_INITIALIZER_BROADCAST (initializer);

  GST_TENSOR_INITIALIZER_UNLOCK (initializer);

  if (G_UNLIKELY (prev_active_sinkpad != active_sinkpad
          || inipad->events_pending)) {
    gst_pad_sticky_events_foreach (GST_PAD_CAST (inipad), forward_sticky_events,
        initializer);
    inipad->events_pending = FALSE;
  }

  if (prev_active_sinkpad) {
    gst_object_unref (prev_active_sinkpad);
    prev_active_sinkpad = NULL;
  }

  /* buf=gst_buffer_ref(buf); */

  res = gst_pad_push (initializer->srcpad, buf);
  printf ("pushed %s [%lu] \n", gst_pad_get_name (active_sinkpad), buf->pts);
  /* if(GST_BUFFER_PTS(buf) == 0){ */
  /*   GstPad *changepad = gst_element_get_static_pad ((GstElement*)initializer, "sink_1"); */
  /*   gst_tensor_initializer_set_active_pad(initializer, changepad); */
  /*   gst_object_unref(changepad); */
  /* } */

  GST_TENSOR_INITIALIZER_LOCK (initializer);

  inipad->pushed = TRUE;

  GST_TENSOR_INITIALIZER_UNLOCK (initializer);

done:
  if (prev_active_sinkpad)
    gst_object_unref (prev_active_sinkpad);
  prev_active_sinkpad = NULL;
  return res;

ignore:
  {
    gboolean active_pad_pushed =
        GST_INITIALIZER_PAD_CAST (active_sinkpad)->pushed;
    printf ("ignored : %s %lu\n", gst_pad_get_name (pad), GST_BUFFER_PTS (buf));

    inipad->discont = TRUE;
    GST_TENSOR_INITIALIZER_UNLOCK (initializer);
    gst_buffer_unref (buf);

    GST_OBJECT_LOCK (inipad);
    if (!active_pad_pushed)
      res = GST_FLOW_OK;

    GST_OBJECT_UNLOCK (inipad);

    goto done;
  }

flushing:
  {
    gst_buffer_unref (buf);
    res = GST_FLOW_FLUSHING;
    goto done;
  }
}

static void gst_tensor_initializer_dispose (GObject * object);
static void gst_tensor_initializer_finalize (GObject * object);

static void gst_tensor_initializer_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_tensor_initializer_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstStateChangeReturn gst_tensor_initializer_change_state (GstElement *
    element, GstStateChange transition);

static gboolean gst_tensor_initializer_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

#define _do_init \
    GST_DEBUG_CATEGORY_INIT (gst_tensor_initializer_debug, \
        "tensor_initiazlier", 0, "An tensor initializer element");
#define gst_tensor_initializer_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTensorInitializer, gst_tensor_initializer,
    GST_TYPE_ELEMENT, _do_init);

static GstPad *
gst_tensor_initializer_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * unused, const GstCaps * caps)
{
  GstTensorInitializer *initializer;
  gchar *name = NULL;
  GstPad *sinkpad = NULL;

  g_return_val_if_fail (templ->direction == GST_PAD_SINK, NULL);

  initializer = GST_TENSOR_INITIALIZER (element);

  GST_TENSOR_INITIALIZER_LOCK (initializer);

  GST_LOG_OBJECT (initializer, "Creating new pad sink_%u",
      initializer->padcount);
  name = g_strdup_printf ("sink_%u", initializer->padcount++);
  sinkpad = g_object_new (GST_TYPE_INITIALIZER_PAD,
      "name", name, "direction", templ->direction, "template", templ, NULL);
  g_free (name);

  initializer->n_pads++;

  gst_pad_set_event_function (sinkpad,
      GST_DEBUG_FUNCPTR (gst_initializer_pad_event));
  gst_pad_set_query_function (sinkpad,
      GST_DEBUG_FUNCPTR (gst_initializer_pad_query));
  gst_pad_set_chain_function (sinkpad,
      GST_DEBUG_FUNCPTR (gst_initializer_pad_chain));
  gst_pad_set_iterate_internal_links_function (sinkpad,
      GST_DEBUG_FUNCPTR (gst_initializer_pad_iterate_linked_pads));

  gst_pad_set_active (sinkpad, TRUE);
  gst_element_add_pad (GST_ELEMENT (initializer), sinkpad);
  GST_TENSOR_INITIALIZER_UNLOCK (initializer);

  return sinkpad;
}

static void
gst_tensor_initializer_release_pad (GstElement * element, GstPad * pad)
{
  GstTensorInitializer *initializer;

  initializer = GST_TENSOR_INITIALIZER (element);
  GST_LOG_OBJECT (initializer, "Releasing pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  GST_TENSOR_INITIALIZER_LOCK (initializer);
  /* if the pad was the active pad, makes us select a new one */
  if (initializer->active_sinkpad == pad) {
    GST_DEBUG_OBJECT (initializer, "Deactivating pad %s:%s",
        GST_DEBUG_PAD_NAME (pad));
    gst_object_unref (initializer->active_sinkpad);
    initializer->active_sinkpad = NULL;
  }
  initializer->n_pads--;
  GST_TENSOR_INITIALIZER_UNLOCK (initializer);

  gst_pad_set_active (pad, FALSE);
  gst_element_remove_pad (GST_ELEMENT (initializer), pad);
}


/**
 * @brief initialize the tensor_initializer's class
 */
static void
gst_tensor_initializer_class_init (GstTensorInitializerClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->dispose = gst_tensor_initializer_dispose;
  gobject_class->finalize = gst_tensor_initializer_finalize;

  gobject_class->set_property = gst_tensor_initializer_set_property;
  gobject_class->get_property = gst_tensor_initializer_get_property;


  g_object_class_install_property (gobject_class, PROP_PASSTHROUGH,
      g_param_spec_boolean ("passthrough", "Passthrough",
          "Flag to pass incoming buufer", FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          TRUE, G_PARAM_READWRITE));

  gst_element_class_set_details_simple (gstelement_class,
      "tensor_initializer",
      "Test/Tensor",
      "Get Tensors and Re-construct tensor to check",
      "Jijoong Moon <jijoong.moon@samsung.com>");

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &sink_factory, GST_TYPE_INITIALIZER_PAD);
  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);

  gstelement_class->request_new_pad = gst_tensor_initializer_request_new_pad;
  gstelement_class->release_pad = gst_tensor_initializer_release_pad;
  gstelement_class->change_state = gst_tensor_initializer_change_state;
}

/**
 * @brief initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_tensor_initializer_init (GstTensorInitializer * initializer)
{
  initializer->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_iterate_internal_links_function (initializer->srcpad,
      GST_DEBUG_FUNCPTR (gst_initializer_pad_iterate_linked_pads));
  gst_pad_set_event_function (initializer->srcpad,
      GST_DEBUG_FUNCPTR (gst_tensor_initializer_event));
  gst_element_add_pad (GST_ELEMENT (initializer), initializer->srcpad);

  initializer->silent = TRUE;
  initializer->passthrough = FALSE;
  initializer->active_sinkpad = NULL;
  initializer->padcount = 0;
  initializer->have_group_id = TRUE;
  g_mutex_init (&initializer->lock);
  g_cond_init (&initializer->cond);
  initializer->eos = FALSE;
}

static void
gst_tensor_initializer_dispose (GObject * object)
{
  GstTensorInitializer *initializer = GST_TENSOR_INITIALIZER (object);
  if (initializer->active_sinkpad) {
    gst_object_unref (initializer->active_sinkpad);
    initializer->active_sinkpad = NULL;
  }
  G_OBJECT_CLASS (parent_class)->dispose (object);
}


static void
gst_tensor_initializer_finalize (GObject * object)
{

  GstTensorInitializer *initializer = GST_TENSOR_INITIALIZER (object);
  g_mutex_clear (&initializer->lock);
  g_cond_clear (&initializer->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_tensor_initializer_set_active_pad (GstTensorInitializer * self,
    GstPad * pad)
{
  GstInitializerPad *old, *new;
  GstPad **active_pad_p;

  if (pad == self->active_sinkpad)
    return FALSE;
  if (pad != NULL) {
    g_return_val_if_fail (GST_PAD_IS_SINK (pad), FALSE);
    g_return_val_if_fail (GST_IS_INITIALIZER_PAD (pad), FALSE);
    g_return_val_if_fail (GST_PAD_PARENT (pad) == GST_ELEMENT_CAST (self),
        FALSE);
  }

  old = GST_INITIALIZER_PAD_CAST (self->active_sinkpad);
  new = GST_INITIALIZER_PAD_CAST (pad);

  if (old)
    old->pushed = FALSE;
  if (new)
    new->pushed = FALSE;

  if (old != new && new)
    new->events_pending = TRUE;

  active_pad_p = &self->active_sinkpad;
  gst_object_replace ((GstObject **) active_pad_p, GST_OBJECT_CAST (pad));

  if (old && old != new)
    gst_pad_push_event (GST_PAD_CAST (old), gst_event_new_reconfigure ());
  if (new)
    gst_pad_push_event (GST_PAD_CAST (new), gst_event_new_reconfigure ());

  if (old != new && new && new->eos) {
    new->eos_sent = FALSE;
    GST_TENSOR_INITIALIZER_BROADCAST (self);
  }

  return TRUE;
}


/**
 * @brief set property vmthod
 */
static void
gst_tensor_initializer_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTensorInitializer *filter = GST_TENSOR_INITIALIZER (object);

  switch (prop_id) {
    case PROP_PASSTHROUGH:
      filter->passthrough = g_value_get_boolean (value);
      break;
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * @brief get property vmthod
 */
static void
gst_tensor_initializer_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTensorInitializer *filter = GST_TENSOR_INITIALIZER (object);

  switch (prop_id) {
    case PROP_PASSTHROUGH:
      g_value_set_boolean (value, filter->passthrough);
      break;
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstPad *
gst_tensor_initializer_get_linked_pad (GstTensorInitializer * initializer,
    GstPad * pad, gboolean strict)
{
  GstPad *otherpad = NULL;
  GST_TENSOR_INITIALIZER_LOCK (initializer);
  if (pad == initializer->srcpad)
    otherpad = initializer->active_sinkpad;
  else if (pad == initializer->active_sinkpad || !strict)
    otherpad = initializer->srcpad;
  if (otherpad)
    gst_object_ref (otherpad);
  GST_TENSOR_INITIALIZER_UNLOCK (initializer);

  return otherpad;
}

static gboolean
gst_tensor_initializer_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstTensorInitializer *initializer;
  gboolean result = FALSE;
  GstIterator *iter;
  gboolean done = FALSE;
  GValue item = { 0, };
  GstPad *eventpad;
  GList *pushed_pads = NULL;

  initializer = GST_TENSOR_INITIALIZER (parent);

  iter = gst_element_iterate_sink_pads (GST_ELEMENT_CAST (initializer));

  while (!done) {
    switch (gst_iterator_next (iter, &item)) {
      case GST_ITERATOR_OK:
        eventpad = g_value_get_object (&item);
        if (g_list_find (pushed_pads, eventpad)) {
          g_value_reset (&item);
          break;
        }
        gst_event_ref (event);
        result = gst_pad_push_event (eventpad, event);
        g_value_reset (&item);
        break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (iter);
        break;
      case GST_ITERATOR_ERROR:
        GST_ERROR_OBJECT (pad, "Could not iterate over snkpads");
        done = TRUE;
        break;
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
    }
  }

  g_value_unset (&item);
  gst_iterator_free (iter);

  g_list_free (pushed_pads);

  gst_event_unref (event);

  return result;
}


static inline gboolean
gst_tensor_initializer_is_active_sinkpad (GstTensorInitializer * initializer,
    GstPad * pad)
{
  gboolean res;

  GST_TENSOR_INITIALIZER_LOCK (initializer);
  res = (pad == initializer->active_sinkpad);
  GST_TENSOR_INITIALIZER_UNLOCK (initializer);

  return res;
}

static GstPad *
gst_tensor_initializer_get_active_sinkpad (GstTensorInitializer * initializer)
{
  GstPad *active_sinkpad;
  active_sinkpad = initializer->active_sinkpad;
  printf ("active pad : %s\n", gst_pad_get_name (active_sinkpad));
  if (active_sinkpad == NULL) {
    GValue item = G_VALUE_INIT;
    GstIterator *iter =
        gst_element_iterate_sink_pads (GST_ELEMENT_CAST (initializer));
    GstIteratorResult ires;

    while ((ires = gst_iterator_next (iter, &item)) == GST_ITERATOR_RESYNC)
      gst_iterator_resync (iter);
    if (ires == GST_ITERATOR_OK) {
      active_sinkpad = initializer->active_sinkpad = g_value_dup_object (&item);
      g_value_reset (&item);
    } else
      GST_WARNING_OBJECT (initializer, "Couldn't find a default sink pad");
    gst_iterator_free (iter);
  }

  return active_sinkpad;
}


static void
gst_tensor_initializer_reset (GstTensorInitializer * initializer)
{
  GList *walk;

  GST_TENSOR_INITIALIZER_LOCK (initializer);
  /* clear active pad */
  if (initializer->active_sinkpad) {
    gst_object_unref (initializer->active_sinkpad);
    initializer->active_sinkpad = NULL;
  }
  initializer->eos_sent = FALSE;

  /* reset each of our sinkpads state */
  for (walk = GST_ELEMENT_CAST (initializer)->sinkpads; walk;
      walk = g_list_next (walk)) {
    GstInitializerPad *inipad = GST_INITIALIZER_PAD_CAST (walk->data);

    gst_initializer_pad_reset (inipad);

    if (inipad->tags) {
      gst_tag_list_unref (inipad->tags);
      inipad->tags = NULL;
    }
  }
  initializer->have_group_id = TRUE;
  GST_TENSOR_INITIALIZER_UNLOCK (initializer);
}

static GstStateChangeReturn
gst_tensor_initializer_change_state (GstElement * element,
    GstStateChange transition)
{
  GstTensorInitializer *self = GST_TENSOR_INITIALIZER (element);
  GstStateChangeReturn result;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
      GST_TENSOR_INITIALIZER_LOCK (self);
      GstPad *pad = gst_element_get_static_pad (element, "sink_0");
      self->eos = FALSE;
      self->flushing = FALSE;
      gst_tensor_initializer_set_active_pad (self, pad);
      /* GstCaps *caps = gst_pad_get_current_caps(pad); */
      /* GstStructure *s = gst_caps_get_structure(caps,0); */
      /* printf("in READY_TO_PAUSED : %s \n", gst_structure_get_name(s)); */

      gst_object_unref (pad);
      GST_TENSOR_INITIALIZER_UNLOCK (self);
    }
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
    {
      /* GST_TENSOR_INITIALIZER_LOCK (self); */
      /* GstPad *pad = gst_element_get_static_pad (element, "sink_1"); */
      /* gst_tensor_initializer_set_active_pad(self, pad); */
      /* GstCaps *caps = gst_pad_get_current_caps(pad); */
      /* GstStructure *s = gst_caps_get_structure(caps,0); */
      /* printf("in READY_TO_PAUSED : %s \n", gst_structure_get_name(s)); */
      /* gst_object_unref(pad); */

      /* GST_TENSOR_INITIALIZER_UNLOCK (self); */
    }
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      /* first unlock before we call the parent state change function, which
       * tries to acquire the stream lock when going to ready. */
    {
      GST_TENSOR_INITIALIZER_LOCK (self);
      self->eos = TRUE;
      self->flushing = TRUE;
      GST_TENSOR_INITIALIZER_BROADCAST (self);
      GST_TENSOR_INITIALIZER_UNLOCK (self);
    }
      break;
    default:
      break;
  }

  result = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_tensor_initializer_reset (self);
      break;
    default:
      break;
  }

  return result;
}

/**
 * @brief Function to initialize the plugin.
 *
 * See GstPluginInitFunc() for more details.
 */
NNSTREAMER_PLUGIN_INIT (tensor_initializer)
{
  GST_DEBUG_CATEGORY_INIT (gst_tensor_initializer_debug, "tensor_initializer",
      0, "tensorinitializer");
  return gst_element_register (plugin, "tensor_initializer",
      GST_RANK_NONE, GST_TYPE_TENSOR_INITIALIZER);
}

#ifndef SINGLE_BINARY
/**
 * PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "nnstreamer"
#endif

/**
 * gstreamer looks for this structure to register tensor_initializers
 * exchange the string 'Template tensor_initializer' with your tensor_initializer description
 */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    tensor_initializer,
    "Element tensor_initializer to test tensors",
    tensor_initializer_plugin_init, VERSION, "LGPL", "nnstreamer",
    "https://github.com/nnsuite/nnstreamer/")
#endif
